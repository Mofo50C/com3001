#ifndef QUEUE_H
#define QUEUE_H 1

#include "tm_queue.h"

typedef struct tm_queue tm_queue_t;
typedef int * queue_val_t;

int *queue_val_new(int val);

int print_val(size_t idx, void *value, void *arg);

void print_queue(tm_queue_t *q);

void tm_enqueue_front(tm_queue_t *q, int val);

void tm_enqueue_back(tm_queue_t *q, int val);

void tm_peak_front(tm_queue_t *q);

void tm_peak_back(tm_queue_t *q);

void tm_dequeue_front(tm_queue_t *q);

void tm_dequeue_back(tm_queue_t *q);

#endif