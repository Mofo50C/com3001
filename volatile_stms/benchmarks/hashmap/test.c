#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "map.h"

#define RAII
#include "stm.h" 

struct root {
    tm_hashmap_t *map;
};

static struct root root = {.map = NULL};

struct worker_args {
	int idx;
	int val;
	int key1;
	int key2;
	tm_hashmap_t *map;
};

int main(int argc, char const *argv[])
{
	if (hashmap_new(&root.map)) {
		printf("error in new\n");
		return 1;
	}

	tm_hashmap_t *map = root.map;

	STM_TH_ENTER();

    size_t i;
    for (i = 0; i < 40; i++) {
        // map_insert(map, i, 21);
		tm_map_insert(map, i, 21);
    }

	STM_TH_EXIT();

    print_map(map);

	hashmap_destroy(&root.map);

    return 0;
}
