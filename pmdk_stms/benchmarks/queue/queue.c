#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>

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

TOID(struct queue_val) queue_val_new(int val)
{
	TOID(struct queue_val) myval;
	int mynum = val;
	POBJ_NEW(pop, &myval, struct queue_val, val_constr, &mynum);

	return myval;
}

int print_val(size_t idx, PMEMoid value, void *arg)
{
	printf("%d ", D_RW(TYPED_VAL(value))->val);
	return 0;
}

void print_queue(TOID(struct queue) q)
{
	printf("{ ");
	queue_foreach(q, print_val, NULL);
	printf("}\n");
}

void tm_enqueue_front(TOID(struct queue) q, int val)
{	
	TOID(struct queue_val) qval = queue_val_new(val);
	int res = queue_push_front_tm(q, qval.oid);
	pid_t pid = gettid();
	if (res) {
		printf("[%d] error prepending %d\n", pid, val);
		pmemobj_free(&qval.oid);
		return;
	}

	printf("[%d] prepended %d\n", pid, val);
}

void tm_enqueue_back(TOID(struct queue) q, int val)
{	
	TOID(struct queue_val) qval = queue_val_new(val);
	int res = queue_push_back_tm(q, qval.oid);
	pid_t pid = gettid();
	if (res) {
		printf("[%d] error appending %d\n", pid, val);
		pmemobj_free(&qval.oid);
		return;
	}

	printf("[%d] appended %d\n", pid, val);
}

void tm_peak_front(TOID(struct queue) q)
{
	PMEMoid retval = queue_peak_front_tm(q, NULL);
	if (!OID_IS_NULL(retval))
		printf("[%d] front: %d\n", gettid(), D_RO(TYPED_VAL(retval))->val);
}

void tm_peak_back(TOID(struct queue) q)
{
	PMEMoid retval = queue_peak_back_tm(q, NULL);
	if (!OID_IS_NULL(retval))
		printf("[%d] front: %d\n", gettid(), D_RO(TYPED_VAL(retval))->val);
}

void tm_dequeue_front(TOID(struct queue) q)
{
	int err = 0;
	PMEMoid retval = queue_pop_front_tm(q, &err);
	pid_t pid = gettid();
	if (err) {
		printf("[%d] error dequeuing front\n", pid);
		return;
	}

	if (!OID_IS_NULL(retval))
		printf("[%d] dequeued front: %d\n", gettid(), D_RO(TYPED_VAL(retval))->val);
	else
		printf("[%d] queue empty\n", gettid());
}

void tm_dequeue_back(TOID(struct queue) q)
{
	int err = 0;
	PMEMoid retval = queue_pop_back_tm(q, &err);
	pid_t pid = gettid();
	if (err) {
		printf("[%d] error dequeuing back\n", pid);
		return;
	}

	if (!OID_IS_NULL(retval))
		printf("[%d] dequeued back: %d\n", gettid(), D_RO(TYPED_VAL(retval))->val);
	else
		printf("[%d] queue empty\n", gettid());
}