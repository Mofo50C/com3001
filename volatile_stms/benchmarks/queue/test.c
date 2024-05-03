#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "queue.h"

#define RAII
#include "stm.h" 

struct root {
    tm_queue_t *queue;
};

static struct root root = {.queue = NULL};

struct worker_args {
	int idx;
	int val;
	tm_queue_t *queue;
};

int main(int argc, char const *argv[])
{
	if (argc != 1) {
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	if (queue_new(&root.queue)) {
		printf("error in new\n");
		return 1;
	}

	tm_queue_t *queue = root.queue;

	STM_TH_ENTER();

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

	if (queue_destroy(&root.queue))
		printf("error in destroy\n");

	STM_TH_EXIT();
	
    return 0;
}
