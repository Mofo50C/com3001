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

void *worker_seq_push_back(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args.idx, gettid());
	PTM_TH_ENTER(pop);

	tm_enqueue_back(args.queue, args.val);

	PTM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 3) {
		printf("usage: %s <pool_file> <num_threads>\n", argv[0]);
		return 1;
	}
	
	int num_threads = atoi(argv[2]);

	if (num_threads < 1) {
		printf("<num_threads> must be at least 1\n");
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

	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];
	int i;
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->idx = i + 1;
		args->queue = queue;
		// args->val = rand() % 1000;
		args->val = i + 1;

		pthread_create(&workers[i], NULL, worker_seq_push_back, args);
	}
	
	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	print_queue(queue);

	if (queue_destroy(pop, &rootp->queue))
		printf("error in destroy\n");
	
	pmemobj_close(pop);

    return 0;
}
