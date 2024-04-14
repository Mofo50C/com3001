#ifndef NOREC_HASHMAP_H
#define NOREC_HASHMAP_H 1

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hashmap_entry {
	uint64_t hash;
	uintptr_t key;
	uint64_t index;
	int occupied;
} hashmap_entry_t;

typedef struct hashmap {
	int length;
	int capacity;
	int num_grows;
	hashmap_entry_t *table;
} hashmap_t;


/* init new hashmap */
hashmap_t *hashmap_new(void);

/* inserts only new entry */
int hashmap_insert(hashmap_t *h, uintptr_t key, uint64_t value);

/* inserts or updates */
int hashmap_put(hashmap_t *h, uintptr_t key, uint64_t value);

/* get entry or NULL if key is not present */
hashmap_entry_t *hashmap_get(hashmap_t *h, uintptr_t key);

/* delete key from map */
int hashmap_delete(hashmap_t *h, uintptr_t key);

/* boolean return if key is present */
int hashmap_contains(hashmap_t *h, uintptr_t key);

/* empties map and shrinks to save memory */
void hashmap_empty(hashmap_t *h);

/* entirely destroys hashmap and frees memory */
void hashmap_destroy(hashmap_t *h);

#ifdef __cplusplus
}
#endif

#endif