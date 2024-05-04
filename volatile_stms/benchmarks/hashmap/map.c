#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include "util.h"
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

void print_map(tm_hashmap_t *h)
{
	printf("{\n");
	hashmap_foreach(h, print_val, NULL);
	printf("}\n");
}

void map_insert(tm_hashmap_t *h, uint64_t key, int val)
{	
	int *new_val = hash_val_new(val);
	if (new_val == NULL) {
		printf("failed to allocate new hash_val\n");
		return;
	}

	int err = 0;
	void *oldval = hashmap_put(h, key, (void *)new_val, &err);
	if (err) {
		free(new_val);
		return;
	}

	if (oldval)
		free(oldval);
}

void tm_map_insert(tm_hashmap_t *h, uint64_t key, int val)
{	
	int *new_val = hash_val_new(val);
	if (new_val == NULL) {
		printf("failed to allocate new hash_val\n");
		return;
	}

	int err = 0;
	void *oldval = hashmap_put_tm(h, key, (void *)new_val, &err);
	pid_t pid = gettid();
	if (err) {
		DEBUGPRINT("[%d] error putting %ju => %d\n", pid, key, val);
		free(new_val);
	} else if (oldval) {
		DEBUGPRINT("[%d] update %ju => %d\n", pid, key, val);
		free(oldval);
	} else {
		DEBUGPRINT("[%d] insert %ju => %d\n", pid, key, val);
	}
}

void tm_map_read(tm_hashmap_t *h, uint64_t key)
{
	void *ret = hashmap_get_tm(h, key, NULL);
	if (ret)
		DEBUGPRINT("[%d] get %ju: %d\n", gettid(), key, *(int *)ret);
}

void tm_map_delete(tm_hashmap_t *h, uint64_t key)
{
	int err = 0;
	void *oldval = hashmap_delete_tm(h, key, &err);

	pid_t pid = gettid();
	if (err) {
		DEBUGPRINT("[%d] failed to delete key %ju\n", pid, key);
	} else if (oldval) {
		DEBUGPRINT("[%d] deleted key %ju with val %d\n", pid, key, *(int *)oldval);
		free(oldval);
	} else {
		DEBUGPRINT("[%d] %ju not in map\n", pid, key);
	}
}