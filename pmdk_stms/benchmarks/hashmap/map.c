#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>

#include "map.h"

int val_constr(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct hash_val *val = ptr;
	if (arg)
		val->val = *(int *)arg;
	else
		val->val = 0;

	pmemobj_persist(pop, val, sizeof(*val));
	return 0;
}

TOID(struct hash_val) hash_val_new(int val)
{
	TOID(struct hash_val) myval;
	int mynum = val;
	POBJ_NEW(pop, &myval, struct hash_val, val_constr, &mynum);

	return myval;
}

int print_val(uint64_t key, PMEMoid value, void *arg)
{
	printf("\t%ju => %d\n", key, D_RW(TYPED_VAL(value))->val);

	return 0;
}

int reduce_val(uint64_t key, PMEMoid value, void *arg)
{
	int *total = (int *)arg;
	*total = *total + D_RO(TYPED_VAL(value))->val;
	return 0;
}

void map_insert(TOID(struct hashmap) h, uint64_t key, int val)
{	
	int res;
	TOID(struct hash_val) hval = hash_val_new(val);
	PMEMoid oldval = hashmap_put(pop, h, key, hval.oid, &res);
	if (res < 0) {
		pmemobj_free(&hval.oid);
		return;
	}

	if (!OID_IS_NULL(oldval)) {
		pmemobj_free(&oldval);
	}
}

void tm_map_insert(TOID(struct hashmap) h, uint64_t key, int val)
{	
	int res;
	TOID(struct hash_val) hval = hash_val_new(val);
	PMEMoid oldval = hashmap_put_tm(h, key, hval.oid, &res);
	pid_t pid = gettid();
	if (res < 0) {
		pmemobj_free(&hval.oid);
		printf("[%d] error putting %ju => %d\n", pid, key, val);
		return;
	}

	if (OID_IS_NULL(oldval)) {
		printf("[%d] insert %ju => %d\n", pid, key, val);
	} else {
		pmemobj_free(&oldval);
		printf("[%d] update %ju => %d\n", pid, key, val);
	}
}

void tm_map_read(TOID(struct hashmap) h, uint64_t key)
{
	PMEMoid ret = hashmap_get_tm(h, key, NULL);
	if (!OID_IS_NULL(ret))
		printf("[%d] get %ju: %d\n", gettid(), key, D_RW(TYPED_VAL(ret))->val);
}

void tm_map_delete(TOID(struct hashmap) h, uint64_t key)
{
	int err = 0;
	PMEMoid oldval = hashmap_delete_tm(h, key, &err);

	pid_t pid = gettid();
	if (err) {
		printf("[%d] failed to delete key %ju\n", pid, key);
	} else if (OID_IS_NULL(oldval)) {
		printf("[%d] %ju not in map\n", pid, key);
	} else {
		printf("[%d] deleted key %ju with val %d\n", pid, key, D_RW(TYPED_VAL(oldval))->val);
		pmemobj_free(&oldval);
	}
}

void map_print(TOID(struct hashmap) h)
{
	printf("{\n");
	hashmap_foreach(h, print_val, NULL);
	printf("}\n");
}