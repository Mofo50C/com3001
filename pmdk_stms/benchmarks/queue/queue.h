#ifndef QUEUE_H
#define QUEUE_H 1

#include <libpmemobj.h>
#include "tm_queue.h"

#define QUEUE_TYPE_NUM 1500

TOID_DECLARE(struct queue_val, QUEUE_TYPE_NUM);

struct queue_val {
	int val;
};

typedef TOID(struct tm_queue) tm_queue_t;
typedef TOID(struct queue_val) queue_val_t;

extern PMEMobjpool *pop;

queue_val_t queue_val_new(int val);

int print_val(size_t idx, PMEMoid value, void *arg);

void print_queue(tm_queue_t q);

void tm_enqueue_front(tm_queue_t q, int val);

void tm_enqueue_back(tm_queue_t q, int val);

void tm_peak_front(tm_queue_t q);

void tm_peak_back(tm_queue_t q);

void tm_dequeue_front(tm_queue_t q);

void tm_dequeue_back(tm_queue_t q);

#endif