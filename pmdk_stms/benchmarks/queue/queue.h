#ifndef QUEUE_H
#define QUEUE_H 1

#include <libpmemobj.h>
#include "tm_queue.h"

#define QUEUE_TYPE_NUM 1500

TOID_DECLARE(struct queue_val, QUEUE_TYPE_NUM);

struct queue_val {
	int val;
};

extern PMEMobjpool *pop;

#define TYPED_VAL(val) (TOID(struct queue_val))val

TOID(struct queue_val) queue_val_new(int val);

int print_val(size_t idx, PMEMoid value, void *arg);

void print_queue(TOID(struct queue) q);

void tm_enqueue_front(TOID(struct queue) q, int val);

void tm_enqueue_back(TOID(struct queue) q, int val);

void tm_peak_front(TOID(struct queue) q);

void tm_peak_back(TOID(struct queue) q);

void tm_dequeue_front(TOID(struct queue) q);

void tm_dequeue_back(TOID(struct queue) q);

#endif