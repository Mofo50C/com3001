#ifndef QUEUE_H
#define QUEUE_H 1

#include "tm_queue.h"

typedef struct tm_queue tm_queue_t;

void print_queue(tm_queue_t *q);

void TX_enqueue_front(tm_queue_t *q, int val);

void TX_enqueue_back(tm_queue_t *q, int val);

void TX_peak_front(tm_queue_t *q);

void TX_peak_back(tm_queue_t *q);

void TX_dequeue_front(tm_queue_t *q);

void TX_dequeue_back(tm_queue_t *q);

#endif