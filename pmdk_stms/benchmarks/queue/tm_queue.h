#ifndef TM_QUEUE_H
#define TM_QUEUE_H 1

#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TM_QUEUE_TYPE_NUM 2001

TOID_DECLARE(struct queue, TM_QUEUE_TYPE_NUM);
TOID_DECLARE(struct entry, TM_QUEUE_TYPE_NUM + 1);

struct entry {
	PMEMoid value;
	TOID(struct entry) next;
	TOID(struct entry) prev;
};

struct queue {
	size_t length;
    TOID(struct entry) front;
    TOID(struct entry) back;
};

/* allocate new queue */
int queue_new(PMEMobjpool *pop, TOID(struct queue) *q);

int queue_push_back_tm(TOID(struct queue) q, PMEMoid value);

int queue_push_front_tm(TOID(struct queue) q, PMEMoid value);

PMEMoid queue_pop_back_tm(TOID(struct queue) q, int *err);

PMEMoid queue_pop_front_tm(TOID(struct queue) q, int *err);

PMEMoid queue_peak_back_tm(TOID(struct queue) q, int *err);

PMEMoid queue_peak_front_tm(TOID(struct queue) q, int *err);

/* foreach function with callback */
int queue_foreach(TOID(struct queue) q, int (*cb)(size_t idx, PMEMoid value, void *arg), void *arg);

int queue_foreach_reverse(TOID(struct queue) q, int (*cb)(size_t idx, PMEMoid value, void *arg), void *arg);

/* entirely destroys queue and frees memory */
int queue_destroy(PMEMobjpool *pop, TOID(struct queue) *q);

#ifdef __cplusplus
}
#endif

#endif