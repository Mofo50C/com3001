#define _GNU_SOURCE
#include <unistd.h>

#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "queue.h"

#define RAII
#include "ptm.h" 

#define NANOSEC 1000000000.0

POBJ_LAYOUT_BEGIN(queue_test);
POBJ_LAYOUT_ROOT(queue_test, struct root);
POBJ_LAYOUT_END(queue_test);

struct root {
    tm_queue_t queue;
};

PMEMobjpool *pop;

struct worker_args {
	int idx;
	int val;
	tm_queue_t queue;
};

void *worker_enqueue(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args.idx, gettid());
	PTM_TH_ENTER(pop);

	tm_enqueue_back(args.queue, args.val);

	PTM_TH_EXIT();

	return NULL;
}

void *worker_dequeue(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args.idx, gettid());
	PTM_TH_ENTER(pop);

	tm_dequeue_front(args.queue);

	PTM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 3) {
		printf("usage: %s <pool_file> <num_threads>\n", argv[0]);
		return 1;
	}
	
	int num_push = atoi(argv[2]);

	if (num_push < 1) {
		printf("<num_threads> must be at least 1\n");
		return 1;
	}

	const char *path = argv[1];
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(queue_test),
			PMEMOBJ_MIN_POOL * 40, 0666)) == NULL) {
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

	tm_queue_t queue = rootp->queue;

	pthread_t pushers[num_push];
	pthread_t poppers[num_push];
	struct worker_args push_data[num_push];
	struct worker_args pop_data[num_push];

	srand(time(NULL));
	struct timespec s, f;

	clock_gettime(CLOCK_MONOTONIC, &s);
	int i;
	for (i = 0; i < num_push; i++) {
		struct worker_args *args = &push_data[i];
		args->idx = i + 1;
		args->queue = queue;
		args->val = rand() % 1000;
		// args->val = i + 1;

		pthread_create(&pushers[i], NULL, worker_enqueue, args);
	}

	for (i = 0; i < num_push; i++) {
		struct worker_args *args = &pop_data[i];
		args->idx = i + 1;
		args->queue = queue;
		pthread_create(&poppers[i], NULL, worker_dequeue, args);
	}
	
	for (i = 0; i < num_push; i++) {
		pthread_join(pushers[i], NULL);
	}

	for (i = 0; i < num_push; i++) {
		pthread_join(poppers[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &f);
	double elapsed_time = (f.tv_sec - s.tv_sec);
	elapsed_time += (f.tv_nsec - s.tv_nsec) / NANOSEC;
	printf("Elapsed: %f\n", elapsed_time);

	// print_queue(queue);

	if (queue_destroy(pop, &rootp->queue))
		printf("error in destroy\n");
	
	pmemobj_close(pop);

    return 0;
}
