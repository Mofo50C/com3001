#define _GNU_SOURCE
#include <unistd.h>

#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "map.h"

#define RAII
#include "ptm.h" 

POBJ_LAYOUT_BEGIN(hashmap_transfer);
POBJ_LAYOUT_ROOT(hashmap_transfer, struct root);
POBJ_LAYOUT_END(hashmap_transfer);

struct root {
    TOID(struct hashmap) map;
};

PMEMobjpool *pop;

struct worker_args {
	int idx;
	int val;
	int key1;
	int key2;
	TOID(struct hashmap) map;
};

void *worker_new(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n\tTransfer %d from %d to %d\n", args.idx, gettid(), args.val, args.key1, args.key2);
	PTM_TH_ENTER(pop);

	PMEMoid val1, val2;
	PTM_BEGIN() {
		val1 = hashmap_get_tm(args.map, args.key1, NULL);
		if (OID_IS_NULL(val1))
			PTM_ABORT();

		val2 = hashmap_get_tm(args.map, args.key2, NULL);
		if (OID_IS_NULL(val2))
			PTM_ABORT();

		struct hash_val *valp1 = pmemobj_direct(val1);
		struct hash_val *valp2 = pmemobj_direct(val2);

		TOID(struct hash_val) new1 = PTM_ZNEW(struct hash_val);
		D_RW(new1)->val = PTM_READ_DIRECT(&valp1->val, sizeof(int)) - args.val;
		TOID(struct hash_val) new2 = PTM_ZNEW(struct hash_val);
		D_RW(new2)->val = PTM_READ_DIRECT(&valp2->val, sizeof(int)) + args.val;

		int err = 0;
		hashmap_put_tm(args.map, args.key1, new1.oid, &err);
		if (err)
			PTM_ABORT();

		err = 0;
		hashmap_put_tm(args.map, args.key2, new2.oid, &err);
		if (err)
			PTM_ABORT();

	} PTM_ONCOMMIT {
		pmemobj_free(&val1);
		pmemobj_free(&val2);
	} PTM_ONABORT {
		DEBUGABORT();
	} PTM_END

	PTM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 6) {
		printf("usage: %s <pool_file> <num_threads> <num_accs> <init_balance> <price>\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[2]);
	if (num_threads < 1) {
		printf("<nthreads> must be at least 1\n");
		return 1;
	}

	int num_accs = atoi(argv[3]);
	if (num_accs < 2) {
		printf("<num_accs> must be at least 2\n");
		return 1;
	}

	int init_balance = atoi(argv[4]);
	int price = atoi(argv[5]);

	const char *path = argv[1];
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(hashmap_transfer),
			PMEMOBJ_MIN_POOL * 4, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(hashmap_transfer))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

    TOID(struct root) root = POBJ_ROOT(pop, struct root);
    struct root *rootp = D_RW(root);

	
	if (hashmap_new(pop, &rootp->map)) {
		printf("error in new\n");
		pmemobj_close(pop);
		return 1;
	}

	TOID(struct hashmap) map = rootp->map;

	size_t i;
	for (i = 0; i < num_accs; i++) {
		map_insert(map, i, init_balance);
	}

	int total_before = 0;
	hashmap_foreach(map, reduce_val, &total_before);

	map_print(map);

	srand(time(NULL));
	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->idx = i + 1;
		args->map = map;
		args->val = price;
		args->key1 = rand() % num_accs;
		do {
			args->key2 = rand() % num_accs;
		} while (args->key2 == args->key1);

		pthread_create(&workers[i], NULL, worker_new, args);
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	map_print(map);

	int total_after = 0;
	hashmap_foreach(map, reduce_val, &total_after);

	if (total_after == total_before)
		printf("PASS!\n");
	else
		printf("FAIL!\n");

	if (hashmap_destroy(pop, &rootp->map))
		printf("error in destroy\n");
	
	pmemobj_close(pop);

    return 0;
}
