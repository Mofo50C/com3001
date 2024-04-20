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

void map_insert(struct hashmap *h, uint64_t key, int val)
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

void map_read(struct hashmap *h, uint64_t key)
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

void *map_worker(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("th%d with pid %d\n", args.idx, gettid());
	STM_TH_ENTER();

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

    
	if (hashmap_new(&root.map)) {
		printf("error in new...\n");
		return 1;
	}

	struct hashmap *map = root.map;

	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];

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

	hashmap_destroy(&root.map);

    return 0;
}
