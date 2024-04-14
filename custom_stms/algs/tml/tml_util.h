#ifndef TML_UTIL_H
#define TML_UTIL_H 1
#include <stdlib.h>
#include <setjmp.h>

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

struct tx_data { 
	jmp_buf env;
	struct tx_data *next;
};

struct tx_stack {
	struct tx_data *first;
	struct tx_data *head;
};

struct tx_vec {
	size_t length;
	size_t capacity;
	void **addrs;
};

int tx_stack_init(struct tx_stack **sp);

void tx_stack_push(struct tx_stack *s, struct tx_data *entry);

struct tx_data *tx_stack_pop(struct tx_stack *s);

void tx_stack_empty(struct tx_stack *s);

int tx_stack_destroy(struct tx_stack **sp);

int tx_stack_isempty(struct tx_stack *s);

int tx_vector_init(struct tx_vec **vecp);

int tx_vector_resize(struct tx_vec *vec);

int tx_vector_append(struct tx_vec *vec, void *entry);

void tx_vector_destroy(struct tx_vec *vec);

void tx_vector_empty(struct tx_vec *vec);

#ifdef __cplusplus
}
#endif

#endif