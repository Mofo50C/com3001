#ifndef TX_HASHMAP_H
#define TX_HASHMAP_H 1

#include <stdlib.h>
#include <stdint>

#ifdef __cplusplus
extern "C" {
#endif

struct entry {
	uint64_t key;
	uint64_t hash;
	void *value;
	struct entry *next;
};

struct buckets {
	size_t num_buckets;
	struct entry *arr[];
};

struct hashmap {
	size_t length;
	size_t capacity;
	int num_grows;
	struct buckets *buckets;
};

/* allocate new hashmap */
int hashmap_new(struct hashmap **h);

/* inserts or updates */
int hashmap_put(struct hashmap *h, uint64_t key, void *value, void **retval);

/* get value or NULL if key is not present */
void *hashmap_get(struct hashmap *h, uint64_t key, int *err);

/* delete key from map */
void *hashmap_delete(struct hashmap *h, uint64_t key, int *err);

/* boolean return if key is present */
int hashmap_contains(struct hashmap *h, uint64_t key);

/* foreach function with callback */
int hashmap_foreach(struct hashmap *h, int (*cb)(uint64_t key, void *value, void *arg), void *arg);

/* entirely destroys hashmap and frees memory */
int hashmap_destroy(struct hashmap **h);

#ifdef __cplusplus
}
#endif

#endif