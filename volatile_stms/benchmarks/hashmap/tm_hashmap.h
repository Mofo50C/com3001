#ifndef TM_HASHMAP_H
#define TM_HASHMAP_H 1

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tm_hashmap_entry {
	uint64_t key;
	uint64_t hash;
	int value;
	struct tm_hashmap_entry *next;
};

struct tm_hashmap_buckets {
	size_t num_buckets;
	struct tm_hashmap_entry *arr[];
};

struct tm_hashmap {
	size_t length;
	size_t capacity;
	int num_grows;
	struct tm_hashmap_buckets *buckets;
};

int hashmap_new(struct tm_hashmap **h);

int hashmap_new_cap(struct tm_hashmap **h, size_t capacity);

int hashmap_resize_tm(struct tm_hashmap *h);

int hashmap_resize(struct tm_hashmap *h);

/* returns -1 on error, 1 if updated and 0 if inserted new */
int hashmap_put_tm(struct tm_hashmap *h, uint64_t key, int value, int *retval);

/* returns -1 on error, 1 if updated and 0 if inserted new */
int hashmap_put(struct tm_hashmap *h, uint64_t key, int value, int *retval);

/* returns 1 on error or not found */
int hashmap_get_tm(struct tm_hashmap *h, uint64_t key, int *retval);

/* returns 1 on error or not found */
int hashmap_delete_tm(struct tm_hashmap *h, uint64_t key, int *retval);

/* returns 1 on error or not found */
int hashmap_contains_tm(struct tm_hashmap *h, uint64_t key);

/* foreach function with callback */
int hashmap_foreach(struct tm_hashmap *h, int (*cb)(uint64_t key, int value, void *arg), void *arg);

/* entirely destroys hashmap and frees memory */
void hashmap_destroy(struct tm_hashmap **h);

#ifdef __cplusplus
}
#endif

#endif