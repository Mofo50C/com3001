#ifndef HASHINDEX_H
#define HASHINDEX_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _hashindex_entry {
	uint64_t hash;
	uintptr_t key;
	uint64_t index;
	int occupied;
} hashindex_entry_t;

typedef struct _hashindex {
	int length;
	int capacity;
	int num_grows;
	hashindex_entry_t *table;
} hashindex_t;


/* init new hashindex */
hashindex_t *hashindex_new(void);

/* inserts only new entry */
int hashindex_insert(hashindex_t *h, uintptr_t key, uint64_t value);

/* inserts or updates */
int hashindex_put(hashindex_t *h, uintptr_t key, uint64_t value);

/* get entry or NULL if key is not present */
hashindex_entry_t *hashindex_get(hashindex_t *h, uintptr_t key);

/* delete key from map */
int hashindex_delete(hashindex_t *h, uintptr_t key);

/* boolean return if key is present */
int hashindex_contains(hashindex_t *h, uintptr_t key);

/* empties map and shrinks to save memory */
void hashindex_empty(hashindex_t *h);

/* entirely destroys hashindex and frees memory */
void hashindex_destroy(hashindex_t *h);

#ifdef __cplusplus
}
#endif

#endif