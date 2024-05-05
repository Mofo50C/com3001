#ifndef MAP_H
#define MAP_H 1

#include <libpmemobj.h>
#include "tm_hashmap.h"

typedef TOID(struct tm_hashmap) tm_hashmap_t;

extern PMEMobjpool *pop;

int reduce_val(uint64_t key, int value, void *arg);

void print_map(tm_hashmap_t h);

void map_insert(tm_hashmap_t h, uint64_t key, int val);

void TX_map_insert(tm_hashmap_t h, uint64_t key, int val);

void TX_map_read(tm_hashmap_t h, uint64_t key);

void TX_map_delete(tm_hashmap_t h, uint64_t key);

#endif