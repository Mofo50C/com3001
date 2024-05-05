#define _GNU_SOURCE
#include <unistd.h>

#define RAII
#include "stm.h"
#include "tm_queue.h"

int queue_new(struct tm_queue **q)
{
    struct tm_queue *queue = malloc(sizeof(struct tm_queue));
    if (queue == NULL)
        return 1;

    queue->back = NULL;
    queue->front = NULL;
    queue->length = 0;

    *q = queue;
	return 0;
}

void queue_entry_init(struct tm_queue_entry *e, int value)
{
    e->value = value;
    e->next = NULL;
    e->prev = NULL;
}

struct tm_queue_entry *queue_entry_new_tm(int value)
{
    struct tm_queue_entry *e = STM_ZNEW(struct tm_queue_entry);
    queue_entry_init(e, value);
    return e;
}

int queue_push_back_tm(struct tm_queue *q, int value)
{
    int ret = 0;
    STM_BEGIN() {
        struct tm_queue_entry *new_tail = queue_entry_new_tm(value);
        struct tm_queue_entry *back = STM_READ_FIELD(q, back);
        struct tm_queue_entry *front = STM_READ_FIELD(q, front);
        if (back == NULL && front == NULL) {
            STM_WRITE_FIELD(q, back, new_tail);
            STM_WRITE_FIELD(q, front, new_tail);
        } else {
            new_tail->prev = back;
            STM_WRITE_FIELD(back, next, new_tail);
            STM_WRITE_FIELD(q, back, new_tail);
        }
        STM_WRITE_FIELD(q, length, (STM_READ_FIELD(q, length) + 1));
    } STM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } STM_END

    return ret;
}

int queue_push_front_tm(struct tm_queue *q, int value)
{
    int ret = 0;

    STM_BEGIN() {
        struct tm_queue_entry *new_head = queue_entry_new_tm(value);
        struct tm_queue_entry *back = STM_READ_FIELD(q, back);
        struct tm_queue_entry *front = STM_READ_FIELD(q, front);
        if (back == NULL && front == NULL) {
            STM_WRITE_FIELD(q, back, new_head);
            STM_WRITE_FIELD(q, front, new_head);
        } else {
            new_head->next = front;
            STM_WRITE_FIELD(front, prev, new_head);
            STM_WRITE_FIELD(q, front, new_head);
        }
        STM_WRITE_FIELD(q, length, (STM_READ_FIELD(q, length) + 1));
    } STM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } STM_END

    return ret;
}

int queue_pop_back_tm(struct tm_queue *q, int *retval)
{
    int succ = 0;
    int ret = 0;
    STM_BEGIN() {
        struct tm_queue_entry *back = STM_READ_FIELD(q, back);
        if (STM_READ_FIELD(q, front) == NULL && back == NULL)
            goto end_empty;

        succ = 1;
        if (retval)
            *retval = STM_READ_FIELD(back, value);
        struct tm_queue_entry *prev = STM_READ_FIELD(back, prev);
        if (prev == NULL) {
            STM_WRITE_FIELD(q, front, NULL);
            STM_WRITE_FIELD(q, back, NULL);
        } else {
            STM_WRITE_FIELD(prev, next, NULL);
            STM_WRITE_FIELD(q, back, prev);
        }
        STM_WRITE_FIELD(back, prev, NULL);
        STM_FREE(back);
        size_t len = STM_READ_FIELD(q, length) - 1;
        STM_WRITE_FIELD(q, length, len);
end_empty:  ;
    } STM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } STM_END

    if (ret || !succ)
        return 1;
    
    return 0;
}

int queue_pop_front_tm(struct tm_queue *q, int *retval)
{
    int succ = 0;
    int ret = 0;
    STM_BEGIN() {
        struct tm_queue_entry *front = STM_READ_FIELD(q, front);
        if (front == NULL && STM_READ_FIELD(q, back) == NULL)
            goto end_empty;

        succ = 1;
        if (retval)
            *retval = STM_READ_FIELD(front, value);
        struct tm_queue_entry *next = STM_READ_FIELD(front, next);
        if (next == NULL) {
            STM_WRITE_FIELD(q, front, NULL);
            STM_WRITE_FIELD(q, back, NULL);
        } else {
            STM_WRITE_FIELD(next, prev, NULL);
            STM_WRITE_FIELD(q, front, next);
        }
        STM_WRITE_FIELD(front, next, NULL);
        STM_FREE(front);
        size_t len = STM_READ_FIELD(q, length) - 1;
        STM_WRITE_FIELD(q, length, len);
end_empty:  ;
    } STM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } STM_END

    if (ret || !succ)
        return 1;

    return 0;
}

int queue_peak_back_tm(struct tm_queue *q, int *retval)
{
    int succ = 0;
    int ret = 0;
    STM_BEGIN() {
        struct tm_queue_entry *back = STM_READ_FIELD(q, back);
        if (back != NULL) {
            succ = 1;
            if (retval)
                *retval = STM_READ_FIELD(back, value);
        }
    } STM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } STM_END

    if (ret || !succ)
        return 1;

    return 0;
}

int queue_peak_front_tm(struct tm_queue *q, int *retval)
{
    int succ = 0;
    int ret = 0;
    STM_BEGIN() {
        struct tm_queue_entry *front = STM_READ_FIELD(q, front);
        if (front != NULL) {
            succ = 1;
            if (retval)
                *retval = STM_READ_FIELD(front, value);
        }
    } STM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } STM_END

    if (ret || !succ)
        return 1;

    return 0;
}

int queue_foreach(struct tm_queue *q, int (*cb)(size_t idx, int value, void *arg), void *arg)
{
    int ret = 0;

    int i = 0;
    struct tm_queue_entry *head;
    for (head = q->front; head != NULL; head = head->next) {
        ret = cb(i, head->value, arg);
        if (ret)
            break;
        
        i++;
    }

    return ret;
}

int queue_foreach_reverse(struct tm_queue *q, int (*cb)(size_t idx, int value, void *arg), void *arg)
{
    int ret = 0;

    int i = 0;
    struct tm_queue_entry *tail;
    for (tail = q->back; tail != NULL; tail = tail->prev) {
        ret = cb(i, tail->value, arg);
        if (ret)
            break;
        
        i++;
    }

    return ret;
}

int _queue_destroy_entries(struct tm_queue *q)
{
    struct tm_queue_entry *head;
    while ((head = q->front) != NULL) {
        q->front = head->next;
        free(head);
    }
    
    return 0;
}

int queue_destroy(struct tm_queue **q)
{
    struct tm_queue *queue = *q;
    if (queue == NULL)
        return 0;
    
    if(_queue_destroy_entries(queue))
        return 1;

    free(queue);
    *q = NULL;

    return 0;
}

