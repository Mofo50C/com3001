#ifndef MAP_H
#define MAP_H 1

#include <libpmemobj.h>
#include "tm_hashmap.h"

#define MAP_TYPE_NUM 500

TOID_DECLARE(struct hash_val, MAP_TYPE_NUM);

struct hash_val {
	int val;
};

typedef TOID(struct tm_hashmap) tm_hashmap_t;
typedef TOID(struct hash_val) hash_val_t;

extern PMEMobjpool *pop;

TOID(struct hash_val) hash_val_new(int val);

int print_val(uint64_t key, PMEMoid value, void *arg);

int reduce_val(uint64_t key, PMEMoid value, void *arg);

void map_insert(tm_hashmap_t h, uint64_t key, int val);

void print_map(tm_hashmap_t h);

void tm_map_insert(tm_hashmap_t h, uint64_t key, int val);

void tm_map_read(tm_hashmap_t h, uint64_t key);

void tm_map_delete(tm_hashmap_t h, uint64_t key);

#endif