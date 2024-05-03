#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>

#include "util.h"
#include "queue.h"

int val_constr(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct queue_val *val = ptr;
	if (arg)
		val->val = *(int *)arg;
	else
		val->val = 0;

	pmemobj_persist(pop, val, sizeof(*val));
	return 0;
}

queue_val_t queue_val_new(int val)
{
	queue_val_t myval;
	int mynum = val;
	POBJ_NEW(pop, &myval, struct queue_val, val_constr, &mynum);

	return myval;
}

int print_val(size_t idx, PMEMoid value, void *arg)
{
	printf("%d ", D_RW((queue_val_t)value)->val);
	return 0;
}

void print_queue(tm_queue_t q)
{
	printf("{ ");
	queue_foreach(q, print_val, NULL);
	printf("}\n");
}

void tm_enqueue_front(tm_queue_t q, int val)
{	
	queue_val_t qval = queue_val_new(val);
	int res = queue_push_front_tm(q, qval.oid);
	pid_t pid = gettid();
	if (res) {
		DEBUGPRINT("[%d] error prepending %d", pid, val);
		pmemobj_free(&qval.oid);
		return;
	}

	DEBUGPRINT("[%d] prepended %d", pid, val);
}

void tm_enqueue_back(tm_queue_t q, int val)
{	
	queue_val_t qval = queue_val_new(val);
	int res = queue_push_back_tm(q, qval.oid);
	pid_t pid = gettid();
	if (res) {
		DEBUGPRINT("[%d] error appending %d", pid, val);
		pmemobj_free(&qval.oid);
		return;
	}

	DEBUGPRINT("[%d] appended %d", pid, val);
}

void tm_peak_front(tm_queue_t q)
{
	PMEMoid retval = queue_peak_front_tm(q, NULL);
	if (!OID_IS_NULL(retval))
		DEBUGPRINT("[%d] front: %d", gettid(), D_RO((queue_val_t)retval)->val);
}

void tm_peak_back(tm_queue_t q)
{
	PMEMoid retval = queue_peak_back_tm(q, NULL);
	if (!OID_IS_NULL(retval))
		DEBUGPRINT("[%d] front: %d", gettid(), D_RO((queue_val_t)retval)->val);
}

void tm_dequeue_front(tm_queue_t q)
{
	int err = 0;
	PMEMoid retval = queue_pop_front_tm(q, &err);
	pid_t pid = gettid();
	if (err) {
		DEBUGPRINT("[%d] error dequeuing front", pid);
		return;
	}

	if (!OID_IS_NULL(retval))
		DEBUGPRINT("[%d] dequeued front: %d", gettid(), D_RO((queue_val_t)retval)->val);
	else
		DEBUGPRINT("[%d] queue empty", gettid());
}

void tm_dequeue_back(tm_queue_t q)
{
	int err = 0;
	PMEMoid retval = queue_pop_back_tm(q, &err);
	pid_t pid = gettid();
	if (err) {
		DEBUGPRINT("[%d] error dequeuing back", pid);
		return;
	}

	if (!OID_IS_NULL(retval))
		DEBUGPRINT("[%d] dequeued back: %d", gettid(), D_RO((queue_val_t)retval)->val);
	else
		DEBUGPRINT("[%d] queue empty", gettid());
}