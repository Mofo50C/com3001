#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>

#include "util.h"
#include "map.h"

int reduce_val(uint64_t key, int value, void *arg)
{
	int *total = (int *)arg;
	*total += value;
	return 0;
}

int print_val(uint64_t key, int value, void *arg)
{
	printf("\t%ju => %d\n", key, value);
	return 0;
}

void print_map(tm_hashmap_t h)
{
	printf("{\n");
	hashmap_foreach(h, print_val, NULL);
	printf("}\n");
}

void map_insert(tm_hashmap_t h, uint64_t key, int val)
{
	int oldval;
	switch (hashmap_put_tm(h, key, val, &oldval)) {
		case 0:
			DEBUGPRINT("insert %ju => %d", key, val);
			break;
		case 1:
			DEBUGPRINT("update %ju => %d | old: %d", key, val, oldval);
			break;
		case -1:
			DEBUGPRINT("error putting %ju => %d", key, val);
			break;
	}
}

void TX_map_insert(tm_hashmap_t h, uint64_t key, int val)
{	
	int oldval;
	pid_t pid = gettid();
	switch (hashmap_put_tm(h, key, val, &oldval)) {
		case 0:
			DEBUGPRINT("[%d] insert %ju => %d", pid, key, val);
			break;
		case 1:
			DEBUGPRINT("[%d] update %ju => %d | old: %d", pid, key, val, oldval);
			break;
		case -1:
			DEBUGPRINT("[%d] error putting %ju => %d", pid, key, val);
			break;
	}
}

void TX_map_read(tm_hashmap_t h, uint64_t key)
{
	int retval;
	pid_t pid = gettid();
	if (hashmap_get_tm(h, key, &retval)) {
		DEBUGPRINT("[%d] get: %ju not found", pid, key);
	} else {
		DEBUGPRINT("[%d] get %ju: %d", pid, key, retval);
	}
}

void TX_map_delete(tm_hashmap_t h, uint64_t key)
{
	int oldval;
	pid_t pid = gettid();
	if (hashmap_delete_tm(h, key, &oldval)) {
		DEBUGPRINT("[%d] delete: %ju not found", pid, key);
	} else {
		DEBUGPRINT("[%d] deleted %ju => %d", pid, key, oldval);
	}
}