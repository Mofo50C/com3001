#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>

#include "util.h"
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

hash_val_t hash_val_new(int val)
{
	hash_val_t myval;
	int mynum = val;
	POBJ_NEW(pop, &myval, struct hash_val, val_constr, &mynum);

	return myval;
}

int print_val(uint64_t key, PMEMoid value, void *arg)
{
	printf("\t%ju => %d\n", key, D_RW((hash_val_t)value)->val);

	return 0;
}

void print_map(tm_hashmap_t h)
{
	printf("{\n");
	hashmap_foreach(h, print_val, NULL);
	printf("}\n");
}

int reduce_val(uint64_t key, PMEMoid value, void *arg)
{
	int *total = (int *)arg;
	*total = *total + D_RO((hash_val_t)value)->val;
	return 0;
}

void map_insert(tm_hashmap_t h, uint64_t key, int val)
{	
	hash_val_t hval = hash_val_new(val);
	int err = 0;
	PMEMoid oldval = hashmap_put(pop, h, key, hval.oid, &err);
	if (err) {
		pmemobj_free(&hval.oid);
		return;
	}

	if (!OID_IS_NULL(oldval)) {
		pmemobj_free(&oldval);
	}
}

void tm_map_insert(tm_hashmap_t h, uint64_t key, int val)
{	
	hash_val_t hval = hash_val_new(val);
	int err = 0;
	PMEMoid oldval = hashmap_put_tm(h, key, hval.oid, &err);
	pid_t pid = gettid();
	if (err) {
		DEBUGPRINT("[%d] error putting %ju => %d\n", pid, key, val);
		pmemobj_free(&hval.oid);
		return;
	}

	if (OID_IS_NULL(oldval)) {
		DEBUGPRINT("[%d] insert %ju => %d\n", pid, key, val);
	} else {
		DEBUGPRINT("[%d] update %ju => %d\n", pid, key, val);
		pmemobj_free(&oldval);
	}
}

void tm_map_read(tm_hashmap_t h, uint64_t key)
{
	PMEMoid ret = hashmap_get_tm(h, key, NULL);
	if (!OID_IS_NULL(ret))
		DEBUGPRINT("[%d] get %ju: %d\n", gettid(), key, D_RW((hash_val_t)ret)->val);
}

void tm_map_delete(tm_hashmap_t h, uint64_t key)
{
	int err = 0;
	PMEMoid oldval = hashmap_delete_tm(h, key, &err);

	pid_t pid = gettid();
	if (err) {
		DEBUGPRINT("[%d] failed to delete key %ju\n", pid, key);
	} else if (OID_IS_NULL(oldval)) {
		DEBUGPRINT("[%d] %ju not in map\n", pid, key);
	} else {
		DEBUGPRINT("[%d] deleted key %ju with val %d\n", pid, key, D_RW((hash_val_t)oldval)->val);
		pmemobj_free(&oldval);
	}
}