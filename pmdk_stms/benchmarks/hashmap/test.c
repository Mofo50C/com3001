#define _GNU_SOURCE
#include <unistd.h>

#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "tm_hashmap.h"

#define RAII
#include "stm.h" 

POBJ_LAYOUT_BEGIN(hashmap_test);
POBJ_LAYOUT_ROOT(hashmap_test, struct root);
POBJ_LAYOUT_TOID(hashmap_test, struct hash_val);
POBJ_LAYOUT_END(hashmap_test);

struct hash_val {
	int val;
};

struct root {
    TOID(struct hashmap) map;
};

static PMEMobjpool *pop;

int val_constr(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct hash_val *val = ptr;
	if (arg)
		val->val = *(int *)arg;
	else
		val->val = 0;

	pmemobj_persist(pop, val, sizeof(*val));
	return 0;
}

TOID(struct hash_val) hash_val_new(int val)
{
	TOID(struct hash_val) myval;
	int mynum = val;
	POBJ_NEW(pop, &myval, struct hash_val, val_constr, &mynum);

	return myval;
}

int print_val(uint64_t key, PMEMoid value, void *arg)
{
	struct hash_val *val = (struct hash_val *)pmemobj_direct(value); 
	printf("\t%d => %d\n", key, val->val);

	return 0;
}

void map_print(TOID(struct hashmap) h)
{
	printf("{\n");
	hashmap_foreach(h, print_val, NULL);
	printf("}\n");
}


void tm_map_insert(TOID(struct hashmap) h, uint64_t key, int val)
{	
	PMEMoid oldval;
	TOID(struct hash_val) hval = hash_val_new(val);
	int res = hashmap_put_tm(h, key, hval.oid, &oldval);
	pid_t pid = gettid();
	switch (res)
	{
	case 1:
		pmemobj_free(&oldval);
		printf("[%d] update %d => %d\n", pid, key, val);
		break;
	case 0:
		printf("[%d] insert %d => %d\n", pid, key, val);
		break;
	default:
		pmemobj_free(&hval.oid);
		printf("[%d] error putting %d => %d\n", pid, key, val);
		break;
	}
}

void tm_map_read(TOID(struct hashmap) h, uint64_t key)
{
	PMEMoid ret = hashmap_get_tm(h, key, NULL);
	if (!OID_IS_NULL(ret))
		printf("[%d] get %d: %d\n", gettid(), key, ((struct hash_val *)pmemobj_direct(ret))->val);
}

void tm_map_delete(TOID(struct hashmap) h, uint64_t key)
{
	int err = 0;
	PMEMoid oldval = hashmap_delete_tm(h, key, &err);

	pid_t pid = gettid();
	if (OID_IS_NULL(oldval) && err) {
		printf("[%d] failed to delete key %d\n", pid, key);
	} else if (OID_IS_NULL(oldval)) {
		printf("[%d] %d not in map\n", pid, key);
	} else {
		printf("[%d] deleted key %d with val %d\n", pid, key, ((struct hash_val *)pmemobj_direct(oldval))->val);
	}
}

struct worker_args {
	int idx;
	int val;
	int key;
	TOID(struct hashmap) map;
};

void *worker_insert(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("th%d with pid %d\n", args.idx, tid);
	STM_TH_ENTER(pop);

	tm_map_insert(args.map, args.key, args.val);

	STM_TH_EXIT();

	return NULL;
}

void *worker_delete(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("th%d with pid %d\n", args.idx, tid);
	STM_TH_ENTER(pop);

	tm_map_delete(args.map, args.key);

	STM_TH_EXIT();

	return NULL;
}

void *worker_get(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("th%d with pid %d\n", args.idx, tid);
	STM_TH_ENTER(pop);

	tm_map_read(args.map, args.key);

	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 6) {
		printf("usage: %s <pool_file> <num_threads> <num_keys> <puts> <gets>\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[2]);

	if (num_threads < 1) {
		printf("<nthreads> must be at least 1\n");
		return 1;
	}

	int num_keys = atoi(argv[3]);
	int const_mult = 100;
	int put_perc = atoi(argv[4]) * const_mult;
	int get_perc = atoi(argv[5]) * const_mult;

	const char *path = argv[1];
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(hashmap_test),
			PMEMOBJ_MIN_POOL * 4, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(hashmap_test))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

    TOID(struct root) root = POBJ_ROOT(pop, struct root);
    struct root *rootp = D_RW(root);
	
	if (hashmap_new(pop, &rootp->map)) {
		printf("error in new...\n");
		pmemobj_close(pop);
		return 1;
	}

	TOID(struct hashmap) map = rootp->map;
	srand(time(NULL));

	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];
	int i;
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->map = map;
		args->val = rand() % 1000;
		args->key = rand() % num_keys;
		args->idx = i + 1;

		int r = rand() % (100 * const_mult);
		if (r < put_perc) {
			pthread_create(&workers[i], NULL, worker_insert, args);
		} else if (r >= put_perc && r < get_perc + put_perc) {
			pthread_create(&workers[i], NULL, worker_get, args);
		} else if (r >= put_perc + get_perc) {
			pthread_create(&workers[i], NULL, worker_delete, args);
		}
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	map_print(map);

	if (hashmap_destroy(pop, &rootp->map))
		printf("error in destroy...\n");

	pmemobj_close(pop);

    return 0;
}
