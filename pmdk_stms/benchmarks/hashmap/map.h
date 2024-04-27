#ifndef MAP_H
#define MAP_H 1

#include <libpmemobj.h>
#include "tm_hashmap.h"

#define MAP_TYPE_NUM 500

TOID_DECLARE(struct hash_val, MAP_TYPE_NUM);

struct hash_val {
	int val;
};

extern PMEMobjpool *pop;

#define TYPED_VAL(val) (TOID(struct hash_val))val

TOID(struct hash_val) hash_val_new(int val);

int print_val(uint64_t key, PMEMoid value, void *arg);

int reduce_val(uint64_t key, PMEMoid value, void *arg);

void map_insert(TOID(struct hashmap) h, uint64_t key, int val);

void map_print(TOID(struct hashmap) h);

void tm_map_insert(TOID(struct hashmap) h, uint64_t key, int val);

void tm_map_read(TOID(struct hashmap) h, uint64_t key);

void tm_map_delete(TOID(struct hashmap) h, uint64_t key);

#endif