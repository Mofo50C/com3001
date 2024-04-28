#define _GNU_SOURCE
#include <unistd.h>

#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "queue.h"

#define RAII
#include "ptm.h" 

POBJ_LAYOUT_BEGIN(queue_test);
POBJ_LAYOUT_ROOT(queue_test, struct root);
POBJ_LAYOUT_END(queue_test);

struct root {
    TOID(struct queue) queue;
};

PMEMobjpool *pop;

struct worker_args {
	int idx;
	int val;
	TOID(struct queue) queue;
};

int main(int argc, char const *argv[])
{
	if (argc != 2) {
		printf("usage: %s <pool_file>\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(queue_test),
			PMEMOBJ_MIN_POOL * 4, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(queue_test))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

    TOID(struct root) root = POBJ_ROOT(pop, struct root);
    struct root *rootp = D_RW(root);

	
	if (queue_new(pop, &rootp->queue)) {
		printf("error in new\n");
		pmemobj_close(pop);
		return 1;
	}

	TOID(struct queue) queue = rootp->queue;

	PTM_TH_ENTER(pop);

	size_t i;
	for (i = 0; i < 5; i++) {
		tm_enqueue_back(queue, i);
		print_queue(queue);
	}

	for (i = 0; i < 5; i++) {
		tm_dequeue_front(queue);
		print_queue(queue);
	}
	
	tm_dequeue_back(queue);

	if (queue_destroy(pop, &rootp->queue))
		printf("error in destroy\n");

	PTM_TH_EXIT();
	
	pmemobj_close(pop);

    return 0;
}
