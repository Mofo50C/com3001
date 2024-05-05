#ifndef TM_HASHMAP_H
#define TM_HASHMAP_H 1

#include <stdint.h>
#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TM_HASHMAP_TYPE_NUM 1001

TOID_DECLARE(struct tm_hashmap, TM_HASHMAP_TYPE_NUM);
TOID_DECLARE(struct tm_hashmap_entry, TM_HASHMAP_TYPE_NUM + 1);
TOID_DECLARE(struct tm_hashmap_buckets, TM_HASHMAP_TYPE_NUM + 2);

struct tm_hashmap_entry {
	uint64_t key;
	uint64_t hash;
	int value;
	TOID(struct tm_hashmap_entry) next;
};

struct tm_hashmap_buckets {
	size_t num_buckets;
	TOID(struct tm_hashmap_entry) arr[];
};

struct tm_hashmap {
	size_t length;
	size_t capacity;
	int num_grows;
	TOID(struct tm_hashmap_buckets) buckets;
};

int hashmap_new(PMEMobjpool *pop, TOID(struct tm_hashmap) *h);

int hashmap_new_cap(PMEMobjpool *pop, TOID(struct tm_hashmap) *h, size_t capacity);

int hashmap_resize(PMEMobjpool *pop, TOID(struct tm_hashmap) h);

int hashmap_resize_tm(TOID(struct tm_hashmap) h);

/* returns -1 on error, 1 if updated and 0 if inserted new */
int hashmap_put(PMEMobjpool *pop, TOID(struct tm_hashmap) h, uint64_t key, int value, int *oldval);

/* returns -1 on error, 1 if updated and 0 if inserted new */
int hashmap_put_tm(TOID(struct tm_hashmap) h, uint64_t key, int value, int *retval);

/* returns 1 on error or not found */
int hashmap_get_tm(TOID(struct tm_hashmap) h, uint64_t key, int *retval);

/* returns 1 on error or not found */
int hashmap_delete_tm(TOID(struct tm_hashmap) h, uint64_t key, int *retval);

/* returns 1 on error or not found */
int hashmap_contains_tm(TOID(struct tm_hashmap) h, uint64_t key);

/* foreach function with callback */
int hashmap_foreach(TOID(struct tm_hashmap) h, int (*cb)(uint64_t key, int value, void *arg), void *arg);

/* entirely destroys hashmap and frees memory */
int hashmap_destroy(PMEMobjpool *pop, TOID(struct tm_hashmap) *h);

#ifdef __cplusplus
}
#endif

#endif