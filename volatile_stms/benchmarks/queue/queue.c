#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "queue.h"

int *queue_val_new(int val)
{
	int *myval = malloc(sizeof(int));
	if (myval == NULL)
		return NULL;

	*myval = val;
	return myval;
}

int print_val(size_t idx, void *value, void *arg)
{
	printf("%d ", *(int *)value);
	return 0;
}

void print_queue(tm_queue_t *q)
{
	printf("{ ");
	queue_foreach(q, print_val, NULL);
	printf("}\n");
}

void tm_enqueue_front(tm_queue_t *q, int val)
{	
	int *qval = queue_val_new(val);
	int res = queue_push_front_tm(q, (void *)qval);
	pid_t pid = gettid();
	if (res) {
		DEBUGPRINT("[%d] error prepending %d", pid, val);
		free(qval);
		return;
	}

	DEBUGPRINT("[%d] prepended %d", pid, val);
}

void tm_enqueue_back(tm_queue_t *q, int val)
{	
	int *qval = queue_val_new(val);
	int res = queue_push_back_tm(q, (void *)qval);
	pid_t pid = gettid();
	if (res) {
		DEBUGPRINT("[%d] error appending %d", pid, val);
		free(qval);
		return;
	}

	DEBUGPRINT("[%d] appended %d", pid, val);
}

void tm_peak_front(tm_queue_t *q)
{
	void *retval = queue_peak_front_tm(q, NULL);
	if (retval)
		DEBUGPRINT("[%d] front: %d", gettid(), *(int *)retval);
}

void tm_peak_back(tm_queue_t *q)
{
	void *retval = queue_peak_back_tm(q, NULL);
	if (retval)
		DEBUGPRINT("[%d] front: %d", gettid(), *(int *)retval);
}

void tm_dequeue_front(tm_queue_t *q)
{
	int err = 0;
	void *retval = queue_pop_front_tm(q, &err);
	pid_t pid = gettid();
	if (err) {
		DEBUGPRINT("[%d] error dequeuing front", pid);
		return;
	}

	if (retval)
		DEBUGPRINT("[%d] dequeued front: %d", gettid(), *(int *)retval);
	else
		DEBUGPRINT("[%d] queue empty", gettid());
}

void tm_dequeue_back(tm_queue_t *q)
{
	int err = 0;
	void *retval = queue_pop_back_tm(q, &err);
	pid_t pid = gettid();
	if (err) {
		DEBUGPRINT("[%d] error dequeuing front", pid);
		return;
	}

	if (retval)
		DEBUGPRINT("[%d] dequeued front: %d", gettid(), *(int *)retval);
	else
		DEBUGPRINT("[%d] queue empty", gettid());
}