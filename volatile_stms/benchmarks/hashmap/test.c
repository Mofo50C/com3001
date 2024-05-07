#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "map.h"

#define NOREC
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

	STM_BEGIN() {
		STM_ABORT();
	}STM_ONABORT {
		printf("abort first\n");
	} STM_END

	STM_BEGIN() {
	} STM_ONCOMMIT {
		printf("commit second\n");
	} STM_END

	STM_TH_EXIT();

    // print_map(map);

	hashmap_destroy(&root.map);

    return 0;
}
