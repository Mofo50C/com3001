#ifndef NOREC_UTIL_H
#define NOREC_UTIL_H 1
#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

int tx_vector_init(struct tx_vec **vecp);

int tx_vector_resize(struct tx_vec *vec);

int tx_vector_append(struct tx_vec *vec, struct tx_vec_entry *entry);

void tx_vector_destroy(struct tx_vec **vecp);

void tx_vector_empty(struct tx_vec *vec);

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