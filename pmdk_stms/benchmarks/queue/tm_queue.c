#define _GNU_SOURCE
#include <unistd.h>

#define RAII
#include "ptm.h"
#include "tm_queue.h"

int queue_new(PMEMobjpool *pop, TOID(struct tm_queue) *q)
{
	int ret = 0;
	TX_BEGIN(pop) {
		TOID(struct tm_queue) queue = TX_ZNEW(struct tm_queue);
        D_RW(queue)->back = TOID_NULL(struct tm_queue_entry);
        D_RW(queue)->front = TOID_NULL(struct tm_queue_entry);
        D_RW(queue)->length = 0;
		TX_ADD_DIRECT(q);
		*q = queue;
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}

void queue_entry_init(TOID(struct tm_queue_entry) e, int value)
{
    D_RW(e)->value = value;
    D_RW(e)->next = TOID_NULL(struct tm_queue_entry);
    D_RW(e)->prev = TOID_NULL(struct tm_queue_entry);
}

TOID(struct tm_queue_entry) queue_entry_new_tm(int value)
{
    TOID(struct tm_queue_entry) e = PTM_ZNEW(struct tm_queue_entry);
    queue_entry_init(e, value);
    return e;
}

int queue_push_back_tm(TOID(struct tm_queue) q, int value)
{
    int ret = 0;
    PTM_BEGIN() {
        TOID(struct tm_queue_entry) new_tail = queue_entry_new_tm(value);
        TOID(struct tm_queue_entry) back = PTM_READ_FIELD(q, back);
        TOID(struct tm_queue_entry) front = PTM_READ_FIELD(q, front);
        if (TOID_IS_NULL(back) && TOID_IS_NULL(front)) {
            PTM_WRITE_FIELD(q, back, new_tail);
            PTM_WRITE_FIELD(q, front, new_tail);
        } else {
            D_RW(new_tail)->prev = back;
            PTM_WRITE_FIELD(back, next, new_tail);
            PTM_WRITE_FIELD(q, back, new_tail);
        }
        PTM_WRITE_FIELD(q, length, (PTM_READ_FIELD(q, length) + 1));
    } PTM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } PTM_END

    return ret;
}

int queue_push_front_tm(TOID(struct tm_queue) q, int value)
{
    int ret = 0;

    PTM_BEGIN() {
        TOID(struct tm_queue_entry) new_head = queue_entry_new_tm(value);
        TOID(struct tm_queue_entry) back = PTM_READ_FIELD(q, back);
        TOID(struct tm_queue_entry) front = PTM_READ_FIELD(q, front);
        if (TOID_IS_NULL(back) && TOID_IS_NULL(front)) {
            PTM_WRITE_FIELD(q, back, new_head);
            PTM_WRITE_FIELD(q, front, new_head);
        } else {
            D_RW(new_head)->next = front;
            PTM_WRITE_FIELD(front, prev, new_head);
            PTM_WRITE_FIELD(q, front, new_head);
        }
        PTM_WRITE_FIELD(q, length, (PTM_READ_FIELD(q, length) + 1));
    } PTM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } PTM_END

    return ret;
}

int queue_pop_back_tm(TOID(struct tm_queue) q, int *retval)
{
    int succ = 0;
    int ret = 0;
    PTM_BEGIN() {
        TOID(struct tm_queue_entry) back = PTM_READ_FIELD(q, back);
        if (TOID_IS_NULL(PTM_READ_FIELD(q, front)) && TOID_IS_NULL(back))
            goto end_empty;

        succ = 1;
        if (retval)
            *retval = PTM_READ_FIELD(back, value);
        TOID(struct tm_queue_entry) prev = PTM_READ_FIELD(back, prev);
        if (TOID_IS_NULL(prev)) {
            PTM_WRITE_FIELD(q, front, TOID_NULL(struct tm_queue_entry));
            PTM_WRITE_FIELD(q, back, TOID_NULL(struct tm_queue_entry));
        } else {
            PTM_WRITE_FIELD(prev, next, TOID_NULL(struct tm_queue_entry));
            PTM_WRITE_FIELD(q, back, prev);
        }
        PTM_WRITE_FIELD(back, prev, TOID_NULL(struct tm_queue_entry));
        PTM_FREE(back);
        size_t len = PTM_READ_FIELD(q, length) - 1;
        PTM_WRITE_FIELD(q, length, len);
end_empty:  ;
    } PTM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } PTM_END

    if (ret || !succ)
        return 1;
    
    return 0;
}

int queue_pop_front_tm(TOID(struct tm_queue) q, int *retval)
{
    int succ = 0;
    int ret = 0;
    PTM_BEGIN() {
        TOID(struct tm_queue_entry) front = PTM_READ_FIELD(q, front);
        if (TOID_IS_NULL(front) && TOID_IS_NULL(PTM_READ_FIELD(q, back)))
            goto end_empty;

        succ = 1;
        if (retval)
            *retval = PTM_READ_FIELD(front, value);
        TOID(struct tm_queue_entry) next = PTM_READ_FIELD(front, next);
        if (TOID_IS_NULL(next)) {
            PTM_WRITE_FIELD(q, front, TOID_NULL(struct tm_queue_entry));
            PTM_WRITE_FIELD(q, back, TOID_NULL(struct tm_queue_entry));
        } else {
            PTM_WRITE_FIELD(next, prev, TOID_NULL(struct tm_queue_entry));
            PTM_WRITE_FIELD(q, front, next);
        }
        PTM_WRITE_FIELD(front, next, TOID_NULL(struct tm_queue_entry));
        PTM_FREE(front);
        size_t len = PTM_READ_FIELD(q, length) - 1;
        PTM_WRITE_FIELD(q, length, len);
end_empty:  ;
    } PTM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } PTM_END

    if (ret || !succ)
        return 1;

    return 0;
}

int queue_peak_back_tm(TOID(struct tm_queue) q, int *retval)
{
    int succ = 0;
    int ret = 0;
    PTM_BEGIN() {
        TOID(struct tm_queue_entry) back = PTM_READ_FIELD(q, back);
        if (!TOID_IS_NULL(back)) {
            succ = 1;
            if (retval)
                *retval = PTM_READ_FIELD(back, value);
        }
    } PTM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } PTM_END

    if (ret || !succ)
        return 1;

    return 0;
}

int queue_peak_front_tm(TOID(struct tm_queue) q, int *retval)
{
    int succ = 0;
    int ret = 0;
    PTM_BEGIN() {
        TOID(struct tm_queue_entry) front = PTM_READ_FIELD(q, front);
        if (!TOID_IS_NULL(front)) {
            succ = 1;
            if (retval)
                *retval = PTM_READ_FIELD(front, value);
        }
    } PTM_ONABORT {
        DEBUGABORT();
        ret = 1;
    } PTM_END

    if (ret || !succ)
        return 1;

    return 0;
}

int queue_foreach(TOID(struct tm_queue) q, int (*cb)(size_t idx, int value, void *arg), void *arg)
{
    int ret = 0;

    int i = 0;
    TOID(struct tm_queue_entry) head;
    for (head = D_RO(q)->front; !TOID_IS_NULL(head); head = D_RO(head)->next) {
        ret = cb(i, D_RO(head)->value, arg);
        if (ret)
            break;
        
        i++;
    }

    return ret;
}

int queue_foreach_reverse(TOID(struct tm_queue) q, int (*cb)(size_t idx, int value, void *arg), void *arg)
{
    int ret = 0;

    int i = 0;
    TOID(struct tm_queue_entry) tail;
    for (tail = D_RO(q)->back; !TOID_IS_NULL(tail); tail = D_RO(tail)->prev) {
        ret = cb(i, D_RO(tail)->value, arg);
        if (ret)
            break;
        
        i++;
    }

    return ret;
}

int _queue_destroy_entries(PMEMobjpool *pop, TOID(struct tm_queue) q)
{
    int ret = 0;

    TX_BEGIN(pop) {
        TOID(struct tm_queue_entry) head;
        while (!TOID_IS_NULL((head = D_RO(q)->front))) {
            D_RW(q)->front = D_RO(head)->next;
            TX_FREE(head);
        }
    } TX_ONABORT {
        DEBUGABORT();
        ret = 1;
    } TX_END
    
    return ret;
}

int queue_destroy(PMEMobjpool *pop, TOID(struct tm_queue) *q)
{
    TOID(struct tm_queue) queue = *q;
    if (TOID_IS_NULL(queue))
        return 0;
    
    if(_queue_destroy_entries(pop, *q))
        return 1;

    int ret = 0;
    TX_BEGIN(pop) {
        TX_ADD_DIRECT(q);
        *q = TOID_NULL(struct tm_queue);
        TX_FREE(queue);
    } TX_ONABORT {
        DEBUGABORT();
        ret = 1;
    } TX_END

    return ret;
}

