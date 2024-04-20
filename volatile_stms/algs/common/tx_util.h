#ifndef TX_UTIL_H
#define TX_UTIL_H 1
#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>

#include <util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASSERT_IN_STAGE(tx, _stage)\
{\
	if (tx->stage != _stage) {\
		DEBUGLOG("expected " #_stage);\
		abort();\
	}\
}

#define ASSERT_IN_WORK(stage)\
{\
	if (stage != TX_STAGE_WORK) {\
		DEBUGLOG("expected to be in TX_STAGE_WORK");\
		abort();\
	}\
}

struct tx_data { 
	jmp_buf env;
	struct tx_data *next;
};

struct tx_stack {
	struct tx_data *first;
	struct tx_data *head;
};

struct tx_vec_entry {
	void *pval;
	void *addr;
	size_t size;
};

struct tx_vec {
	size_t length;
	size_t capacity;
	struct tx_vec_entry *arr;
};

int tx_stack_init(struct tx_stack **sp);

void tx_stack_push(struct tx_stack *s, struct tx_data *entry);

struct tx_data *tx_stack_pop(struct tx_stack *s);

void tx_stack_empty(struct tx_stack *s);

int tx_stack_destroy(struct tx_stack **sp);

int tx_vector_init(struct tx_vec **vecp);

int tx_stack_isempty(struct tx_stack *s);

int tx_vector_resize(struct tx_vec *vec);

int tx_vector_append(struct tx_vec *vec, struct tx_vec_entry *entry);

void tx_vector_destroy(struct tx_vec **vecp);

void tx_vector_empty(struct tx_vec *vec);

void tx_vector_empty_unsafe(struct tx_vec *vec);

int tx_util_is_zeroed(const void *addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif