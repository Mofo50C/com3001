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
#define CONST_MULT 100
#define POOL_SIZE PMEMOBJ_MIN_POOL * 100

POBJ_LAYOUT_BEGIN(queue_test);
POBJ_LAYOUT_ROOT(queue_test, struct root);
POBJ_LAYOUT_END(queue_test);

struct root {
    tm_queue_t queue;
};

PMEMobjpool *pop;

struct worker_args {
	int idx;
	int num_threads;
	int n_rounds;
	tm_queue_t queue;
};

void *worker_enqueue(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	PTM_TH_ENTER(pop);

	int i;
	for (i = 0; i < args->n_rounds; i++) {
		int val = args->idx * args->num_threads * args->n_rounds + i;
		int split = rand() % 2;
		// if (split) {
		// 	TX_enqueue_back(args->queue, val);
		// } else {
		// 	TX_enqueue_front(args->queue, val);
		// }
		TX_enqueue_back(args->queue, val);
	}

	PTM_TH_EXIT();

	return NULL;
}

void *worker_peak(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	PTM_TH_ENTER(pop);

	int i;
	for (i = 0; i < args->n_rounds; i++) {
		int split = rand() % 2;
		// if (split) {
		// 	TX_peak_back(args->queue);
		// } else {
		// 	TX_peak_front(args->queue);
		// }
		TX_peak_front(args->queue);
	}

	PTM_TH_EXIT();

	return NULL;
}

void *worker_dequeue(void *arg)
{
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	PTM_TH_ENTER(pop);

	int i;
	for (i = 0; i < args->n_rounds; i++) {		
		int split = rand() % 2;
		// if (split) {
		// 	TX_dequeue_back(args->queue);
		// } else {
		// 	TX_dequeue_front(args->queue);
		// }
		TX_dequeue_front(args->queue);
	}

	PTM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc < 4) {
		printf("usage: %s <pool_file> <num_threads> <num_rounds> [inserts] [reads] [deletes]\n", argv[0]);
		return 1;
	}
	
	int num_threads = atoi(argv[2]);

	if (num_threads < 1) {
		printf("<num_threads> must be at least 1\n");
		return 1;
	}

	int n_rounds = atoi(argv[3]);
	
	if (n_rounds < 1) {
		printf("<num_rounds> must be at least 1\n");
		return 1;
	}

	const char *path = argv[1];
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(queue_test),
			POOL_SIZE, 0666)) == NULL) {
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

	int put_ratio = 100;
	if (argc >= 5)
		put_ratio = atoi(argv[4]);

	int get_ratio = 0;
	if (argc >= 6)
		get_ratio = atoi(argv[5]);
	
	int del_ratio = 0;
	if (argc == 7)
		del_ratio = atoi(argv[6]);

	if (put_ratio + get_ratio + del_ratio != 100) { 
		printf("ratios must add to 100\n");
		return 1;
	}

	put_ratio *= CONST_MULT;
	get_ratio *= CONST_MULT;
	del_ratio *= CONST_MULT;

    TOID(struct root) root = POBJ_ROOT(pop, struct root);
    struct root *rootp = D_RW(root);

	PTM_STARTUP();

	if (queue_new(pop, &rootp->queue)) {
		printf("error in new\n");
		pmemobj_close(pop);
		return 1;
	}

	tm_queue_t queue = rootp->queue;

	pthread_t workers[num_threads];
	struct worker_args arg_data[num_threads];

	srand(time(NULL));

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	int i;
	for (i = 0; i < num_threads; i++) {
		struct worker_args *args = &arg_data[i];
		args->idx = i + 1;
		args->queue = queue;

		args->n_rounds = n_rounds;
		args->num_threads = num_threads;

		int r = rand() % (100 * CONST_MULT);
		if (r < put_ratio) {
			pthread_create(&workers[i], NULL, worker_enqueue, args);
		} else if (r >= put_ratio && r < get_ratio + put_ratio) {
			pthread_create(&workers[i], NULL, worker_peak, args);
		} else if (r < put_ratio + get_ratio + del_ratio) {
			pthread_create(&workers[i], NULL, worker_dequeue, args);
		}
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}
	clock_gettime(CLOCK_MONOTONIC, &f);
	double elapsed_time = (f.tv_sec - s.tv_sec);
	elapsed_time += (f.tv_nsec - s.tv_nsec) / NANOSEC;
	
	print_queue(queue);
	printf("Elapsed: %f\n", elapsed_time);

	if (queue_destroy(pop, &rootp->queue))
		printf("error in destroy\n");

	PTM_SHUTDOWN();

	pmemobj_close(pop);

    return 0;
}
