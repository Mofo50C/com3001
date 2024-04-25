#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "tm_hashmap.h"

#define RAII
#include "stm.h" 

struct root {
    struct hashmap *map;
};

static struct root root = {.map = NULL};

int print_val(uint64_t key, void *value, void *arg)
{
	int val = *(int *)value; 
	printf("\t%d => %d\n", key, val);

	return 0;
}

void map_print(struct hashmap *h)
{
	printf("{\n");
	hashmap_foreach(h, print_val, NULL);
	printf("}\n");
}

void tm_map_insert(struct hashmap *h, uint64_t key, int val)
{	
	void *oldval;
	int *valptr = malloc(sizeof(int));
	*valptr = val;
	int res = hashmap_put_tm(h, key, (void *)valptr, &oldval);
	switch (res)
	{
	case 1:
		free(oldval);
		printf("update %d => %d\n", key, val);
		break;
	case 0:
		printf("insert %d => %d\n", key, val);
		break;
	default:
		printf("error putting %d\n", key);
		break;
	}
}

void tm_map_delete(struct hashmap *h, uint64_t key)
{
	int err = 0;
	void *oldval = hashmap_delete_tm(h, key, &err);

	if (oldval == NULL && err) {
		printf("failed to delete key %d\n", key);
	} else if (oldval == NULL) {
		printf("%d not in map\n", key);
	} else {
		printf("deleted key %d with val %d\n", key, *(int *)oldval);
	}
}

void tm_map_read(struct hashmap *h, uint64_t key)
{
	void *ret = hashmap_get_tm(h, key, NULL);
	if (ret != NULL)
		printf("get %d: %d\n", key, *(int *)ret);
}

struct worker_args {
	int idx;
	int val;
	int key;
	struct hashmap *map;
};

void *worker_insert(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("th%d with pid %d\n", args.idx, gettid());
	STM_TH_ENTER();

	tm_map_insert(args.map, args.key, args.val);

	STM_TH_EXIT();

	return NULL;
}

void *worker_get(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("th%d with pid %d\n", args.idx, gettid());
	
	STM_TH_ENTER();

	tm_map_read(args.map, args.key);

	STM_TH_EXIT();

	return NULL;
}

void *worker_delete(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("th%d with pid %d\n", args.idx, gettid());
	STM_TH_ENTER();

	tm_map_delete(args.map, args.key);

	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 5) {
		printf("usage: %s <num_threads> <num_keys> <puts> <gets>\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[1]);

	if (num_threads < 1) {
		printf("<nthreads> must be at least 1\n");
		return 1;
	}

	int num_keys = atoi(argv[2]);
	int const_mult = 100;
	int put_perc = atoi(argv[3]) * const_mult;
	int get_perc = atoi(argv[4]) * const_mult;

    
	if (hashmap_new_cap(&root.map, 193)) {
		printf("error in new...\n");
		return 1;
	}

	struct hashmap *map = root.map;
	srand(time(NULL));

	// int num_keys = num_threads / 2;

	// STM_TH_ENTER();

	// int i;
	// for (i = 0; i < num_keys; i++) {
	// 	int *valptr = malloc(sizeof(int));
	// 	*valptr = rand() % 1000;
	// 	hashmap_put_tm(map, i, (void *)valptr, NULL);
	// }
	
	// STM_TH_EXIT();

	// printf("Map before...\n");
	// map_print(map);

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

	printf("\n\nMap after...\n");
	map_print(map);

	hashmap_destroy(&root.map);

    return 0;
}
