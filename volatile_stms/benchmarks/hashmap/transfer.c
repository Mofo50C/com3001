#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "map.h"

#define RAII
#include "stm.h" 

struct root {
    struct hashmap *map;
};

static struct root root = {.map = NULL};

struct worker_args {
	int idx;
	int val;
	int key1;
	int key2;
	struct hashmap *map;
};

void *worker_new(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n\tTransfer %d from %d to %d\n", args.idx, gettid(), args.val, args.key1, args.key2);
	STM_TH_ENTER();

	void *val1, *val2;
	STM_BEGIN() {
		val1 = hashmap_get_tm(args.map, args.key1, NULL);
		if (val1 == NULL)
			STM_ABORT();

		val2 = hashmap_get_tm(args.map, args.key2, NULL);
		if (val2 == NULL)
			STM_ABORT();

		int *new1 = STM_NEW(int);
		*new1 = STM_READ_DIRECT((int *)val1, sizeof(int)) - args.val;
		int *new2 = STM_NEW(int);
		*new2 = STM_READ_DIRECT((int *)val2, sizeof(int)) + args.val;

		int err = 0;
		hashmap_put_tm(args.map, args.key1, (void *)new1, &err);
		if (err)
			STM_ABORT();

		err = 0;
		hashmap_put_tm(args.map, args.key2, (void *)new2, &err);
		if (err)
			STM_ABORT();

	} STM_ONCOMMIT {
		free(val1);
		free(val2);
	} STM_ONABORT {
		DEBUGABORT();
	} STM_END

	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 5) {
		printf("usage: %s <num_threads> <num_accs> <init_balance> <price>\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[1]);
	if (num_threads < 1) {
		printf("<nthreads> must be at least 1\n");
		return 1;
	}

	int num_accs = atoi(argv[2]);
	if (num_accs < 2) {
		printf("<num_accs> must be at least 2\n");
		return 1;
	}

	int init_balance = atoi(argv[3]);
	int price = atoi(argv[4]);
	
	if (hashmap_new(&root.map)) {
		printf("error in new\n");
		return 1;
	}

	struct hashmap *map = root.map;

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

	hashmap_destroy(&root.map);
	
    return 0;
}
