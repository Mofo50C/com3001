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


void map_insert(TOID(struct hashmap) h, uint64_t key, int val)
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

void map_read(TOID(struct hashmap) h, uint64_t key)
{
	PMEMoid ret = hashmap_get_tm(h, key, NULL);
	if (!OID_IS_NULL(ret))
		printf("get %d: %d\n", key, ((struct hash_val *)pmemobj_direct(ret))->val);
}

struct worker_args {
	int idx;
	int val;
	int key;
	TOID(struct hashmap) map;
};

void *map_worker(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("th%d with pid %d\n", args.idx, tid);
	STM_TH_ENTER(pop);

	map_insert(args.map, args.key, args.val);

	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 2) {
		printf("usage: %s <num_threads>\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[1]);

	if (num_threads < 1) {
		printf("<num_accs> must be at least 1\n");
		return 1;
	}

	const char *path = "/mnt/pmem/hashmap_test";
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

	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];

    TOID(struct root) root = POBJ_ROOT(pop, struct root);
    struct root *rootp = D_RW(root);
	
	if (hashmap_new(pop, &rootp->map)) {
		printf("error in new...\n");
		pmemobj_close(pop);
		return 1;
	}

	TOID(struct hashmap) map = rootp->map;

	srand(time(NULL));
	int i;
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->map = map;
		args->val = rand() % 1000;
		args->key = rand() % 1000;
		args->idx = i + 1;

		pthread_create(&workers[i], NULL, map_worker, args);
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	map_print(map);

	if (hashmap_destroy(pop, &rootp->map))
		printf("error in destroy...\n");


    return 0;
}
