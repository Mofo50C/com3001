#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "bench_utils.h"
#include "queue.h"

#define RAII
#include "stm.h" 

#define CONST_MULT 100

struct root {
    tm_queue_t *queue;
};

static struct root root = {.queue = NULL};

struct worker_args {
	int idx;
	int n_secs;
	int num_threads;
	tm_queue_t *queue;
	int put_ratio;
	int get_ratio;
	int del_ratio;
};

void *worker(void *arg)
{
	clamp_cpu(2);
	struct worker_args *args = (struct worker_args *)arg;
	int put_ratio = args->put_ratio;
	int get_ratio = args->get_ratio;
	int del_ratio = args->del_ratio;

	STM_TH_ENTER();
	
	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	double elapsed;
	do {
		int val = rand() & 1000;
		int split = rand() % 2;
		int r = rand() % (100 * CONST_MULT);
		if (r < put_ratio) {
			(split ? TX_enqueue_back : TX_enqueue_front)(args->queue, val);
		} else if (r >= put_ratio && r < get_ratio + put_ratio) {
			(split ? TX_peak_back : TX_peak_front)(args->queue);
		} else if (r < put_ratio + get_ratio + del_ratio) {
			(split ? TX_dequeue_back : TX_dequeue_front)(args->queue);
		}
	
		clock_gettime(CLOCK_MONOTONIC, &f);
		elapsed = get_elapsed_time(&s, &f);
	} while (elapsed < args->n_secs);

	STM_TH_EXIT();

	return NULL;
}

void *worker_enqueue(void *arg)
{
	clamp_cpu(2);
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	STM_TH_ENTER();
	
	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	double elapsed;
	do {
		int val = rand() & 1000;
		int split = rand() % 2;
		if (split) {
			TX_enqueue_back(args->queue, val);
		} else {
			TX_enqueue_front(args->queue, val);
		}
	
		clock_gettime(CLOCK_MONOTONIC, &f);
		elapsed = get_elapsed_time(&s, &f);
	} while (elapsed < args->n_secs);

	STM_TH_EXIT();

	return NULL;
}

void *worker_peak(void *arg)
{
	clamp_cpu(2);
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	STM_TH_ENTER();

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	double elapsed;
	do {
		int split = rand() % 2;
		if (split) {
			TX_peak_back(args->queue);
		} else {
			TX_peak_front(args->queue);
		}
		clock_gettime(CLOCK_MONOTONIC, &f);
		elapsed = get_elapsed_time(&s, &f);
	} while (elapsed < args->n_secs);

	STM_TH_EXIT();

	return NULL;
}

void *worker_dequeue(void *arg)
{
	clamp_cpu(2);
	struct worker_args *args = (struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args->idx, gettid());
	STM_TH_ENTER();

	struct timespec s, f;
	clock_gettime(CLOCK_MONOTONIC, &s);
	double elapsed;
	do {		
		int split = rand() % 2;
		if (split) {
			TX_dequeue_back(args->queue);
		} else {
			TX_dequeue_front(args->queue);
		}
		clock_gettime(CLOCK_MONOTONIC, &f);
		elapsed = get_elapsed_time(&s, &f);
	} while (elapsed < args->n_secs);

	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc < 3) {
		printf("usage: %s <num_threads> <num_rounds> [inserts] [reads] [deletes]\n", argv[0]);
		return 1;
	}
	
	int num_threads = atoi(argv[1]);
	if (num_threads < 1) {
		printf("<num_threads> must be at least 1\n");
		return 1;
	}

	int n_secs = atoi(argv[2]);
	if (n_secs < 1) {
		printf("<num_rounds> must be at least 1\n");
		return 1;
	}

	int put_ratio = 100;
	if (argc >= 4)
		put_ratio = atoi(argv[3]);

	int get_ratio = 0;
	if (argc >= 5)
		get_ratio = atoi(argv[4]);
	
	int del_ratio = 0;
	if (argc == 6)
		del_ratio = atoi(argv[5]);

	if (put_ratio + get_ratio + del_ratio != 100) { 
		printf("ratios must add to 100\n");
		return 1;
	}

	put_ratio *= CONST_MULT;
	get_ratio *= CONST_MULT;
	del_ratio *= CONST_MULT;

	STM_STARTUP();

	if (queue_new(&root.queue)) {
		printf("error in new\n");
		return 1;
	}

	tm_queue_t *queue = root.queue;

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

		args->n_secs = n_secs;
		args->num_threads = num_threads;

		args->put_ratio = put_ratio;
		args->get_ratio = get_ratio;
		args->del_ratio = del_ratio;

		pthread_create(&workers[i], NULL, worker, args);

		// int r = rand() % (100 * CONST_MULT);
		// if (r < put_ratio) {
		// 	pthread_create(&workers[i], NULL, worker_enqueue, args);
		// } else if (r >= put_ratio && r < get_ratio + put_ratio) {
		// 	pthread_create(&workers[i], NULL, worker_peak, args);
		// } else if (r < put_ratio + get_ratio + del_ratio) {
		// 	pthread_create(&workers[i], NULL, worker_dequeue, args);
		// }
	}
	
	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &f);
	double elapsed_time = get_elapsed_time(&s, &f);

	// print_queue(queue);
	printf("Elapsed: %f\n", elapsed_time);

	if (queue_destroy(&root.queue))
		printf("error in destroy\n");

	STM_SHUTDOWN();
	
    return 0;
}
