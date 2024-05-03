#ifndef MAP_H
#define MAP_H 1

#include "tm_hashmap.h"

typedef struct tm_hashmap tm_hashmap_t;

int *hash_val_new(int val);

int print_val(uint64_t key, void *value, void *arg);

int reduce_val(uint64_t key, void *value, void *arg);

void map_insert(tm_hashmap_t *h, uint64_t key, int val);

void map_print(tm_hashmap_t *h);

void tm_map_insert(tm_hashmap_t *h, uint64_t key, int val);

void tm_map_read(tm_hashmap_t *h, uint64_t key);

void tm_map_delete(tm_hashmap_t *h, uint64_t key);

#endif