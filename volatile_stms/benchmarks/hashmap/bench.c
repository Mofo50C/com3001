#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "map.h"
#include "bench_utils.h"

#define RAII
#include "stm.h" 

#define NANOSEC 1000000000.0
#define CONST_MULT 100

struct root {
    tm_hashmap_t *map;
};

static struct root root = {.map = NULL};

struct worker_args {
	int idx;
	tm_hashmap_t *map;
	int n_secs;
	int num_keys;
	int num_threads;
	int put_ratio;
	int get_ratio;
	int del_ratio;
};

void *worker(void *arg)
{
	clamp_cpu(2);
	struct worker_args *args = (struct worker_args *)arg;
	int put_ratio = args->put_ratio;
	int get_ratio = args->get_ratio;
	int del_ratio = args->del_ratio;

	STM_TH_ENTER();

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	double elapsed;
	do {
		int val = rand() % 1000;
		int key = rand() % args->num_keys;

		int r = rand() % (100 * CONST_MULT);
		if (r < put_ratio) {
			TX_map_insert(args->map, key, val);
		} else if (r >= put_ratio && r < get_ratio + put_ratio) {
			TX_map_read(args->map, key);
		} else if (r < put_ratio + get_ratio + del_ratio) {
			TX_map_delete(args->map, key);
		}

		clock_gettime(CLOCK_MONOTONIC, &f);
		elapsed = get_elapsed_time(&s, &f);
	} while (elapsed < args->n_secs);

	STM_TH_EXIT();

	return NULL;
}

void *worker_insert(void *arg)
{
	clamp_cpu(2);
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d [insert]", args->idx, gettid());
	STM_TH_ENTER();

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	double elapsed;
	do {
		int val = rand() % 1000;
		int key = rand() % args->num_keys;
		TX_map_insert(args->map, key, val);

		clock_gettime(CLOCK_MONOTONIC, &f);
		elapsed = get_elapsed_time(&s, &f);
	} while (elapsed < args->n_secs);

	STM_TH_EXIT();

	return NULL;
}

void *worker_delete(void *arg)
{
	clamp_cpu(2);
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d [delete]", args->idx, gettid());
	STM_TH_ENTER();

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	double elapsed;
	do {
		int key = rand() % args->num_keys;
		TX_map_delete(args->map, key);

		clock_gettime(CLOCK_MONOTONIC, &f);
		elapsed = get_elapsed_time(&s, &f);
	} while (elapsed < args->n_secs);

	STM_TH_EXIT();

	return NULL;
}

void *worker_get(void *arg)
{
	clamp_cpu(2);
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d [get]", args->idx, gettid());
	STM_TH_ENTER();

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	double elapsed;
	do {
		int key = rand() % args->num_keys;
		TX_map_read(args->map, key);

		clock_gettime(CLOCK_MONOTONIC, &f);
		elapsed = get_elapsed_time(&s, &f);
	} while (elapsed < args->n_secs);

	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc < 4) {
		printf("usage: %s <num_threads> <num_keys> <n_secs> [inserts] [read] [dels]\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[1]);

	if (num_threads < 1) {
		printf("<nthreads> must be at least 1\n");
		return 1;
	}

	int num_keys = atoi(argv[2]);

	int n_secs = atoi(argv[3]);
	if (n_secs < 1) {
		printf("<num_secs> must be at least 1\n");
		return 1;
	}

	int put_ratio = 100;
	if (argc >= 5)
		put_ratio = atoi(argv[4]);

	int get_ratio = 0;
	if (argc >= 6)
		get_ratio = atoi(argv[5]);
	
	int del_ratio = 0;
	if (argc == 7)
		del_ratio = atoi(argv[6]);

	if (get_ratio + put_ratio + del_ratio != 100) { 
		printf("ratios must add to 100\n");
		return 1;
	}

	put_ratio *= CONST_MULT;
	get_ratio *= CONST_MULT;
	del_ratio *= CONST_MULT;
	
	STM_STARTUP();
	if (hashmap_new_cap(&root.map, num_keys)) {
		printf("error in new...\n");
		return 1;
	}

	tm_hashmap_t *map = root.map;

	// print_map(map);

	srand(time(NULL));

	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	int i;
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->idx = i + 1;
		args->map = map;
		args->n_secs = n_secs;
		args->num_keys = num_keys;
		args->num_threads = num_threads;

		args->put_ratio = put_ratio;
		args->get_ratio = get_ratio;
		args->del_ratio = del_ratio;

		pthread_create(&workers[i], NULL, worker, args);

		// int r = rand() % (100 * CONST_MULT);
		// if (r < put_ratio) {
		// 	pthread_create(&workers[i], NULL, worker_insert, args);
		// } else if (r >= put_ratio && r < get_ratio + put_ratio) {
		// 	pthread_create(&workers[i], NULL, worker_get, args);
		// } else if (r < put_ratio + get_ratio + del_ratio) {
		// 	pthread_create(&workers[i], NULL, worker_delete, args);
		// }
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &f);
	double elapsed_time = get_elapsed_time(&s, &f);

	// print_map(map);
	printf("Total time: %f\n", elapsed_time);

	hashmap_destroy(&root.map);

	STM_SHUTDOWN();

    return 0;
}
