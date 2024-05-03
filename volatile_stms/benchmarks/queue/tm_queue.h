#ifndef TM_QUEUE_H
#define TM_QUEUE_H 1

#ifdef __cplusplus
extern "C" {
#endif

struct tm_queue_entry {
	void *value;
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

int queue_push_back_tm(struct tm_queue *q, void *value);

int queue_push_front_tm(struct tm_queue *q, void *value);

void *queue_pop_back_tm(struct tm_queue *q, int *err);

void *queue_pop_front_tm(struct tm_queue *q, int *err);

void *queue_peak_back_tm(struct tm_queue *q, int *err);

void *queue_peak_front_tm(struct tm_queue *q, int *err);

/* foreach function with callback */
int queue_foreach(struct tm_queue *q, int (*cb)(size_t idx, void *value, void *arg), void *arg);

int queue_foreach_reverse(struct tm_queue *q, int (*cb)(size_t idx, void *value, void *arg), void *arg);

/* entirely destroys queue and frees memory */
int queue_destroy(struct tm_queue **q);

#ifdef __cplusplus
}
#endif

#endif