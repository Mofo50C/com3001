#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <libpmemobj.h>
#include <time.h>

#include "map.h"

#define RAII
#include "ptm.h" 

#define NANOSEC 1000000000.0
#define CONST_MULT 100
#define POOL_SIZE PMEMOBJ_MIN_POOL * 50

POBJ_LAYOUT_BEGIN(hashmap_bench);
POBJ_LAYOUT_ROOT(hashmap_bench, struct root);
POBJ_LAYOUT_END(hashmap_bench);

struct root {
    tm_hashmap_t map;
};

PMEMobjpool *pop;

struct worker_args {
	int idx;
	tm_hashmap_t map;
	int n_rounds;
	int num_keys;
	int num_threads;
};

void *worker_insert(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d", args->idx, gettid());
	PTM_TH_ENTER(pop);

	int i;
	for (i = 0; i < args->n_rounds; i++) {
		int val = args->idx * args->num_threads * args->n_rounds + i;
		// int val = rand() % 1000;
		int key = rand() % args->num_keys;
		TX_map_insert(args->map, key, val);
	}

	PTM_TH_EXIT();

	return NULL;
}

void *worker_delete(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d", args->idx, gettid());
	PTM_TH_ENTER(pop);

	int i;
	for (i = 0; i < args->n_rounds; i++) {
		int key = rand() % args->num_keys;
		TX_map_delete(args->map, key);
	}

	PTM_TH_EXIT();

	return NULL;
}

void *worker_get(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d", args->idx, gettid());
	PTM_TH_ENTER(pop);

	int i;
	for (i = 0; i < args->n_rounds; i++) {
		int key = rand() % args->num_keys;
		TX_map_read(args->map, key);
	}

	PTM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc < 5) {
		printf("usage: %s <pool_file> <num_threads> <num_keys> <num_rounds> [inserts] [read] [dels]\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[2]);

	if (num_threads < 1) {
		printf("<nthreads> must be at least 1\n");
		return 1;
	}

	int num_keys = atoi(argv[3]);
	int n_rounds = atoi(argv[4]);

	int put_ratio = 100;
	if (argc >= 6)
		put_ratio = atoi(argv[5]);

	int get_ratio = 0;
	if (argc >= 7)
		get_ratio = atoi(argv[6]);
	
	int del_ratio = 0;
	if (argc == 8)
		del_ratio = atoi(argv[7]);

	if (get_ratio + put_ratio + del_ratio != 100) { 
		printf("ratios must add to 100\n");
		return 1;
	}

	put_ratio *= CONST_MULT;
	get_ratio *= CONST_MULT;
	del_ratio *= CONST_MULT;

	const char *path = argv[1];
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(hashmap_bench),
			POOL_SIZE, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(hashmap_bench))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

    TOID(struct root) root = POBJ_ROOT(pop, struct root);
    struct root *rootp = D_RW(root);

	PTM_STARTUP();

	if (hashmap_new(pop, &rootp->map)) {
		printf("error in new...\n");
		pmemobj_close(pop);
		return 1;
	}

	tm_hashmap_t map = rootp->map;

	print_map(map);
	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];

	srand(time(NULL));

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	int i;
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->idx = i + 1;
		args->map = map;
		args->n_rounds = n_rounds;
		args->num_keys = num_keys;
		args->num_threads = num_threads;

		int r = rand() % (100 * CONST_MULT);
		if (r < put_ratio) {
			pthread_create(&workers[i], NULL, worker_insert, args);
		} else if (r >= put_ratio && r < get_ratio + put_ratio) {
			pthread_create(&workers[i], NULL, worker_get, args);
		} else if (r < put_ratio + get_ratio + del_ratio) {
			pthread_create(&workers[i], NULL, worker_delete, args);
		}
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &f);
	double elapsed_time = (f.tv_sec - s.tv_sec);
	elapsed_time += (f.tv_nsec - s.tv_nsec) / NANOSEC;

	print_map(map);
	printf("Elapsed: %f\n", elapsed_time);

	if (hashmap_destroy(pop, &rootp->map))
		printf("error in destroy...\n");

	PTM_SHUTDOWN();
	
	pmemobj_close(pop);

    return 0;
}
