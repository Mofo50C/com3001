#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include "map.h"

int *hash_val_new(int val)
{
	int *myval = malloc(sizeof(int));
	if (myval == NULL)
		return NULL;

	*myval = val;
	return myval;
}

int reduce_val(uint64_t key, void *value, void *arg)
{
	int *total = (int *)arg;
	*total = *total + *(int *)value;
	return 0;
}

int print_val(uint64_t key, void *value, void *arg)
{
	int val = *(int *)value; 
	printf("\t%ju => %d\n", key, val);

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
	int res = 0;
	int *new_val = hash_val_new(val);
	if (new_val == NULL) {
		printf("failed to allocate new hash_val\n");
		return;
	}

	void *oldval = hashmap_put(h, key, (void *)new_val, &res);
	if (res < 0) {
		free(new_val);
		return;
	}

	if (oldval) {
		free(oldval);
	}
}

void tm_map_insert(struct hashmap *h, uint64_t key, int val)
{	
	int res = 0;
	int *new_val = hash_val_new(val);
	if (new_val == NULL) {
		printf("failed to allocate new hash_val\n");
		return;
	}

	void *oldval = hashmap_put_tm(h, key, (void *)new_val, &res);
	pid_t pid = gettid();
	if (res < 0) {
		free(new_val);
		printf("[%d] error putting %ju => %d\n", pid, key, val);
		return;
	}

	if (oldval == NULL) {
		printf("[%d] insert %ju => %d\n", pid, key, val);
	} else {
		free(oldval);
		printf("[%d] update %ju => %d\n", pid, key, val);
	}
}

void tm_map_read(struct hashmap *h, uint64_t key)
{
	void *ret = hashmap_get_tm(h, key, NULL);
	if (ret)
		printf("[%d] get %ju: %d\n", gettid(), key, *(int *)ret);
}

void tm_map_delete(struct hashmap *h, uint64_t key)
{
	int err = 0;
	void *oldval = hashmap_delete_tm(h, key, &err);

	pid_t pid = gettid();
	if (err) {
		printf("[%d] failed to delete key %ju\n", pid, key);
	} else if (oldval == NULL) {
		printf("[%d] %ju not in map\n", pid, key);
	} else {
		printf("[%d] deleted key %ju with val %d\n", pid, key, *(int *)oldval);
		free(oldval);
	}
}