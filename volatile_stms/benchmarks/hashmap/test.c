#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tx_hashmap.h"

#define RAII
#include "stm.h" 

struct hash_val {
	int val;
};

struct root {
    struct hashmap *map;
};

static struct root root = {.map = NULL};

int print_val(uint64_t key, void *value, void *arg)
{
	int val = *(int *)value; 
	printf("\t%d => %d\n", key, val);

	return 0;
}

int print_hashmap(struct hashmap *h)
{
	printf("{\n");
	hashmap_foreach(h, print_val, NULL);
	printf("}\n");
}

void insert_value(struct hashmap *h, uint64_t key, int val)
{	
	void *oldval;
	int *valptr = malloc(sizeof(int));
	*valptr = val;
	int res = hashmap_put(h, key, (void *)valptr, &oldval);
	switch (res)
	{
	case 1:
		free(&oldval);
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

void read_value(struct hashmap *h, uint64_t key)
{
	void *ret = hashmap_get(h, key, NULL);
	if (ret != NULL)
		printf("get %d: %d\n", key, *(int *)ret);
}

int main(int argc, char const *argv[])
{

	STM_TH_ENTER();
    
	if (hashmap_new(&root.map))
		printf("error in new...\n");

	struct hashmap *map = root.map;

	insert_value(map, 3, 21);
	read_value(map, 3);
	
	print_hashmap(map);

	insert_value(map, 4, 69);
	insert_value(map, 3, 35);
	
	print_hashmap(map);

	if (hashmap_destroy(&root.map))
		printf("error in destroy...\n");

	STM_TH_EXIT();

    return 0;
}
