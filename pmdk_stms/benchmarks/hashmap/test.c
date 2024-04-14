#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tx_hashmap.h"

#define RAII
#include "stm.h" 

POBJ_LAYOUT_BEGIN(hashmap_test);
POBJ_LAYOUT_ROOT(hashmap_test, struct root);
POBJ_LAYOUT_TOID(hashmap_test, struct hash_val);
POBJ_LAYOUT_END(hashmap_test);

struct hash_val {
	int val;
};

struct root {
    TOID(struct hashmap) map;
};

static PMEMobjpool *pop;

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

int print_val(uint64_t key, PMEMoid value, void *arg)
{
	struct hash_val *val = (struct hash_val *)pmemobj_direct(value); 
	printf("\t%d => %d\n", key, val->val);

	return 0;
}

int print_hashmap(TOID(struct hashmap) h)
{
	printf("{\n");
	hashmap_foreach(h, print_val, NULL);
	printf("}\n");
}


TOID(struct hash_val) new_hash_val(int val)
{
	TOID(struct hash_val) myval;
	int mynum = val;
	POBJ_NEW(pop, &myval, struct hash_val, val_constr, &mynum);

	return myval;
}

void insert_value(TOID(struct hashmap) h, uint64_t key, int val)
{	
	PMEMoid oldval;
	int res = hashmap_put(h, key, new_hash_val(val).oid, &oldval);
	switch (res)
	{
	case 1:
		pmemobj_free(&oldval);
		printf("update %d => %d\n", key, val);
		break;
	case 0:
		printf("insert %d => %d\n", key, val);
		break;
	default:
		printf("error putting %d\n", key);
		break;
	}
}

void read_value(TOID(struct hashmap) h, uint64_t key)
{
	PMEMoid ret = hashmap_get(h, key, NULL);
	if (!OID_IS_NULL(ret))
		printf("get %d: %d\n", key, ((struct hash_val *)pmemobj_direct(ret))->val);
}

int main(int argc, char const *argv[])
{
	const char *path = "/mnt/pmem/hashmap_test";
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(hashmap_test),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(hashmap_test))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

    TOID(struct root) root = POBJ_ROOT(pop, struct root);
    struct root *rootp = D_RW(root);

	STM_TH_ENTER(pop);
    
	if (hashmap_new(&rootp->map))
		printf("error in new...\n");

	TOID(struct hashmap) map = rootp->map;

	insert_value(map, 3, 21);
	read_value(map, 3);
	
	print_hashmap(map);

	insert_value(map, 4, 69);
	insert_value(map, 3, 35);
	
	print_hashmap(map);

	if (hashmap_destroy(&rootp->map))
		printf("error in destroy...\n");

	pmemobj_close(pop);

    return 0;
}
