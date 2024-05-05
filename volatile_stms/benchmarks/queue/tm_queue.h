#ifndef TM_QUEUE_H
#define TM_QUEUE_H 1

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tm_queue_entry {
	int value;
	struct tm_queue_entry *next;
	struct tm_queue_entry *prev;
};

struct tm_queue {
	size_t length;
    struct tm_queue_entry *front;
    struct tm_queue_entry *back;
};

/* allocate new queue */
int queue_new(struct tm_queue **q);

/* returns 0 if successful, otherwise 1 */
int queue_push_back_tm(struct tm_queue *q, int value);

/* returns 0 if successful, otherwise 1 */
int queue_push_front_tm(struct tm_queue *q, int value);

/* returns 0 if successful, otherwise 1 */
int queue_pop_back_tm(struct tm_queue *q, int *retval);

/* returns 0 if successful, otherwise 1 */
int queue_pop_front_tm(struct tm_queue *q, int *retval);

/* returns 0 if successful, otherwise 1 */
int queue_peak_back_tm(struct tm_queue *q, int *retval);

/* returns 0 if successful, otherwise 1 */
int queue_peak_front_tm(struct tm_queue *q, int *retval);

/* foreach function with callback */
int queue_foreach(struct tm_queue *q, int (*cb)(size_t idx, int value, void *arg), void *arg);

int queue_foreach_reverse(struct tm_queue *q, int (*cb)(size_t idx, int value, void *arg), void *arg);

/* entirely destroys queue and frees memory */
int queue_destroy(struct tm_queue **q);

#ifdef __cplusplus
}
#endif

#endif