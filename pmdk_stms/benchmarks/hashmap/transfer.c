#define _GNU_SOURCE
#include <unistd.h>

#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "map.h"

#define RAII
#include "ptm.h" 
#include "bench_utils.h"

POBJ_LAYOUT_BEGIN(hashmap_transfer);
POBJ_LAYOUT_ROOT(hashmap_transfer, struct root);
POBJ_LAYOUT_END(hashmap_transfer);

struct root {
    tm_hashmap_t map;
};

PMEMobjpool *pop;

struct worker_args {
	int idx;
	int val;
	int key1;
	int key2;
	int sleep;
	tm_hashmap_t map;
};

void *worker_new(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n\tTransfer %d from %d to %d\n", args.idx, gettid(), args.val, args.key1, args.key2);
	
	PTM_TH_ENTER(pop);

	PTM_BEGIN() {
		int val1;
		if (hashmap_get_tm(args.map, args.key1, &val1))
			PTM_ABORT();

		int val2;
		if (hashmap_get_tm(args.map, args.key2, &val2))
			PTM_ABORT();

		val1 -= args.val;
		val2 += args.val;

		hashmap_put_tm(args.map, args.key1, val1, NULL);
		hashmap_put_tm(args.map, args.key2, val2, NULL);
	} PTM_ONABORT {
		DEBUGPRINT("[%d] key not found", gettid());
	} PTM_END

	PTM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 7) {
		printf("usage: %s <pool_file> <num_threads> <sleep> <num_accs> <init_balance> <price>\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[2]);
	if (num_threads < 1) {
		printf("<nthreads> must be at least 1\n");
		return 1;
	}

	int msecs = atoi(argv[3]);
	if (msecs < 0) {
		printf("<sleep> cannot be negative milliseconds\n");
		return 1;
	}

	int num_accs = atoi(argv[4]);
	if (num_accs < 2) {
		printf("<num_accs> must be at least 2\n");
		return 1;
	}

	int init_balance = atoi(argv[5]);
	int price = atoi(argv[6]);

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

	tm_hashmap_t map = rootp->map;

	size_t i;
	for (i = 0; i < num_accs; i++) {
		map_insert(map, i, init_balance);
	}

	int total_before = 0;
	hashmap_foreach(map, reduce_val, &total_before);

	print_map(map);

	srand(time(NULL));
	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->idx = i + 1;
		args->sleep = msecs;
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

	print_map(map);

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
