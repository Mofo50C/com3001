#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "queue.h"

#define RAII
#include "stm.h" 

#define NANOSEC 1000000000.0


struct root {
    tm_queue_t *queue;
};

static struct root root = {.queue = NULL};

struct worker_args {
	int idx;
	int val;
	tm_queue_t *queue;
};

void *worker_enqueue(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args.idx, gettid());
	STM_TH_ENTER();

	tm_enqueue_back(args.queue, args.val);

	STM_TH_EXIT();

	return NULL;
}

void *worker_dequeue(void *arg)
{
	struct worker_args args = *(struct worker_args *)arg;
	DEBUGPRINT("<P%d> with pid %d\n", args.idx, gettid());
	STM_TH_ENTER();

	tm_dequeue_front(args.queue);

	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 2) {
		printf("usage: %s <num_threads>\n", argv[0]);
		return 1;
	}
	
	int num_push = atoi(argv[1]);

	if (num_push < 1) {
		printf("<num_threads> must be at least 1\n");
		return 1;
	}

	if (queue_new(&root.queue)) {
		printf("error in new\n");
		return 1;
	}

	tm_queue_t *queue = root.queue;

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

	if (queue_destroy(&root.queue))
		printf("error in destroy\n");
	
    return 0;
}
