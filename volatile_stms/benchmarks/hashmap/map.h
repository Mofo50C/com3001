#ifndef MAP_H
#define MAP_H 1

#include "tm_hashmap.h"

struct hash_val {
	int val;
};

int *hash_val_new(int val);

int print_val(uint64_t key, void *value, void *arg);

int reduce_val(uint64_t key, void *value, void *arg);

void map_insert(struct hashmap *h, uint64_t key, int val);

void map_print(struct hashmap *h);

void tm_map_insert(struct hashmap *h, uint64_t key, int val);

void tm_map_read(struct hashmap *h, uint64_t key);

void tm_map_delete(struct hashmap *h, uint64_t key);

#endif