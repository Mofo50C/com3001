#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "queue.h"

int print_val(size_t idx, int value, void *arg)
{
	printf("%d ", value);
	return 0;
}

void print_queue(tm_queue_t *q)
{
	printf("{ ");
	queue_foreach(q, print_val, NULL);
	printf("}\n");
}

void TX_enqueue_front(tm_queue_t *q, int val)
{	
	pid_t pid = gettid();
	if (queue_push_front_tm(q, val)) {
		DEBUGPRINT("[%d] failed to enqueue %d to front", pid, val);
	} else {
		DEBUGPRINT("[%d] enqueued %d to front", pid, val);
	}
}

void TX_enqueue_back(tm_queue_t *q, int val)
{
	pid_t pid = gettid();
	if (queue_push_back_tm(q, val)) {
		DEBUGPRINT("[%d] failed to enqueue %d to back", pid, val);
	} else {
		DEBUGPRINT("[%d] enqueued %d to back", pid, val);
	}
}

void TX_peak_front(tm_queue_t *q)
{
	int val;
	pid_t pid = gettid();
	if (queue_peak_front_tm(q, &val)) {
		DEBUGPRINT("[%d] peak front failed, not found", pid);
	} else {
		DEBUGPRINT("[%d] front: %d", pid, val);
	}
}

void TX_peak_back(tm_queue_t *q)
{
	int val;
	pid_t pid = gettid();
	if (queue_peak_back_tm(q, &val)) {
		DEBUGPRINT("[%d] peak back failed, not found", pid);
	} else {
		DEBUGPRINT("[%d] back: %d", pid, val);
	}
}

void TX_dequeue_front(tm_queue_t *q)
{
	int val;
	pid_t pid = gettid();
	if (queue_pop_front_tm(q, &val)) {
		DEBUGPRINT("[%d] failed to dequeue front", pid);
	} else {
		DEBUGPRINT("[%d] dequeued %d from front", pid, val);
	}
}

void TX_dequeue_back(tm_queue_t *q)
{
	int val;
	pid_t pid = gettid();
	if (queue_pop_back_tm(q, &val)) {
		DEBUGPRINT("[%d] failed to dequeue back", pid);
	} else {
		DEBUGPRINT("[%d] dequeued %d from back", pid, val);
	}
}