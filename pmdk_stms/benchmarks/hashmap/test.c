#define _GNU_SOURCE
#include <unistd.h>

#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "map.h"

#define RAII
#include "ptm.h" 

POBJ_LAYOUT_BEGIN(hashmap_test);
POBJ_LAYOUT_ROOT(hashmap_test, struct root);
POBJ_LAYOUT_END(hashmap_test);

struct root {
    tm_hashmap_t map;
};

PMEMobjpool *pop;

struct worker_args {
	int idx;
	int val;
	int key1;
	int key2;
	tm_hashmap_t map;
};

int main(int argc, char const *argv[])
{
	if (argc != 2) {
		printf("usage: %s <pool_file>\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(hashmap_test),
			PMEMOBJ_MIN_POOL * 4, 0666)) == NULL) {
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

	
	if (hashmap_new(pop, &rootp->map)) {
		printf("error in new\n");
		pmemobj_close(pop);
		return 1;
	}

	tm_hashmap_t map = rootp->map;

	PTM_TH_ENTER(pop);

    size_t i;
    for (i = 0; i < 40; i++) {
        // map_insert(map, i, 21);
		tm_map_insert(map, i, 21);
    }

	PTM_TH_EXIT();

    print_map(map);

	if (hashmap_destroy(pop, &rootp->map))
		printf("error in destroy\n");
	
	pmemobj_close(pop);

    return 0;
}
