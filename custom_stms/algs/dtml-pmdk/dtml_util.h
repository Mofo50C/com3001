#ifndef DTML_UTIL_H
#define DTML_UTIL_H 1
#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>

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
	if (stage != DTML_STAGE_WORK) {\
		DEBUGLOG("expected to be in DTML_STAGE_WORK");\
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

struct tx_hash_entry {
	uint64_t hash;
	uintptr_t key;
	uint64_t index;
	int occupied;
};

struct tx_hash {
	int length;
	int capacity;
	int num_grows;
	struct tx_hash_entry *table;
};

struct tx_hash_entry {
	uint64_t hash;
	uintptr_t key;
	uint64_t index;
	int occupied;
};

struct tx_hash {
	int length;
	int capacity;
	int num_grows;
	struct tx_hash_entry *table;
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

/* init new tx_hash */
int tx_hash_new(struct tx_hash **hashp);

/* inserts only new entry */
int tx_hash_insert(struct tx_hash *h, uintptr_t key, uint64_t value);

/* inserts or updates */
int tx_hash_put(struct tx_hash *h, uintptr_t key, uint64_t value);

/* get entry or NULL if key is not present */
struct tx_hash_entry *tx_hash_get(struct tx_hash *h, uintptr_t key);

/* delete key from map */
int tx_hash_delete(struct tx_hash *h, uintptr_t key);

/* boolean return if key is present */
int tx_hash_contains(struct tx_hash *h, uintptr_t key);

/* empties map and shrinks to save memory */
void tx_hash_empty(struct tx_hash *h);

/* entirely destroys tx_hash and frees memory */
void tx_hash_destroy(struct tx_hash **hashp);


#ifdef __cplusplus
}
#endif

#endif