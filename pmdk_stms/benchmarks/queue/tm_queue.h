#ifndef TM_QUEUE_H
#define TM_QUEUE_H 1

#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TM_QUEUE_TYPE_NUM 2001

TOID_DECLARE(struct tm_queue, TM_QUEUE_TYPE_NUM);
TOID_DECLARE(struct tm_queue_entry, TM_QUEUE_TYPE_NUM + 1);

struct tm_queue_entry {
	int value;
	TOID(struct tm_queue_entry) next;
	TOID(struct tm_queue_entry) prev;
};

struct tm_queue {
	size_t length;
    TOID(struct tm_queue_entry) front;
    TOID(struct tm_queue_entry) back;
};

/* allocate new queue */
int queue_new(PMEMobjpool *pop, TOID(struct tm_queue) *q);

/* returns 0 if successful, otherwise 1 */
int queue_push_back_tm(TOID(struct tm_queue) q, int value);

/* returns 0 if successful, otherwise 1 */
int queue_push_front_tm(TOID(struct tm_queue) q, int value);

/* returns 0 if successful, otherwise 1 */
int queue_pop_back_tm(TOID(struct tm_queue) q, int *retval);

/* returns 0 if successful, otherwise 1 */
int queue_pop_front_tm(TOID(struct tm_queue) q, int *retval);

/* returns 0 if successful, otherwise 1 */
int queue_peak_back_tm(TOID(struct tm_queue) q, int *retval);

/* returns 0 if successful, otherwise 1 */
int queue_peak_front_tm(TOID(struct tm_queue) q, int *retval);

/* foreach function with callback */
int queue_foreach(TOID(struct tm_queue) q, int (*cb)(size_t idx, int value, void *arg), void *arg);

int queue_foreach_reverse(TOID(struct tm_queue) q, int (*cb)(size_t idx, int value, void *arg), void *arg);

/* entirely destroys queue and frees memory */
int queue_destroy(PMEMobjpool *pop, TOID(struct tm_queue) *q);

#ifdef __cplusplus
}
#endif

#endif