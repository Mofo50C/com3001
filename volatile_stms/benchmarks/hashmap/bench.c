#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "map.h"

#define RAII
#include "stm.h" 

struct root {
    tm_hashmap_t *map;
};

static struct root root = {.map = NULL};

struct worker_args {
	int idx;
	int val;
	int key;
	tm_hashmap_t *map;
	int n_rounds;
	int num_keys;
	int num_threads;
};

void *worker_insert(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	STM_TH_ENTER();

	int i;
	for (i = 0; i < args->n_rounds; i++)
	{
		// int val = args->idx * args->num_threads * args->n_rounds + i;
		int val = rand() % 1000;
		int key = rand() % args->num_keys;
		tm_map_insert(args->map, key, val);
	}

	STM_TH_EXIT();

	return NULL;
}

void *worker_delete(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	STM_TH_ENTER();

	int i;
	for (i = 0; i < args->n_rounds; i++)
	{
		int key = rand() % args->num_keys;
		tm_map_delete(args->map, key);
	}

	STM_TH_EXIT();

	return NULL;
}

void *worker_get(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	STM_TH_ENTER();

	int i;
	for (i = 0; i < args->n_rounds; i++)
	{
		int key = rand() % args->num_keys;
		tm_map_read(args->map, key);
	}

	STM_TH_EXIT();

	return NULL;
}

#define CONST_MULT 100

int main(int argc, char const *argv[])
{
	if (argc < 4) {
		printf("usage: %s <num_threads> <num_keys> <n_rounds> [inserts] [read] [dels]\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[1]);

	if (num_threads < 1) {
		printf("<nthreads> must be at least 1\n");
		return 1;
	}

	int num_keys = atoi(argv[2]);
	int n_rounds = atoi(argv[3]);

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
	if (hashmap_new(&root.map)) {
		printf("error in new...\n");
		return 1;
	}

	tm_hashmap_t *map = root.map;

	map_print(map);

	srand(time(NULL));

	int i;
	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->idx = i + 1;
		args->map = map;
		// args->val = rand() % 1000;
		// args->val = i + 1;
		// args->key = rand() % num_keys;
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

	map_print(map);

	hashmap_destroy(&root.map);

	STM_SHUTDOWN();

    return 0;
}
