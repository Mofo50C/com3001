#ifndef TM_HASHMAP_H
#define TM_HASHMAP_H 1

#include <stdint.h>
#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TM_HASHMAP_TYPE_NUM 1001

TOID_DECLARE(struct hashmap, TM_HASHMAP_TYPE_NUM);
TOID_DECLARE(struct entry, TM_HASHMAP_TYPE_NUM + 1);
TOID_DECLARE(struct buckets, TM_HASHMAP_TYPE_NUM + 2);

struct entry {
	uint64_t key;
	uint64_t hash;
	PMEMoid value;
	TOID(struct entry) next;
};

struct buckets {
	size_t num_buckets;
	TOID(struct entry) arr[];
};

struct hashmap {
	size_t length;
	size_t capacity;
	int num_grows;
	TOID(struct buckets) buckets;
};

/* allocate new hashmap */
int hashmap_new(PMEMobjpool *pop, TOID(struct hashmap) *h);

/* allocate with capacity */
int hashmap_new_cap(PMEMobjpool *pop, TOID(struct hashmap) *h, size_t capacity);

int hashmap_resize(PMEMobjpool *pop, TOID(struct hashmap) h);

int hashmap_resize_tm(TOID(struct hashmap) h);

/* insert without TM */
PMEMoid hashmap_put(PMEMobjpool *pop, TOID(struct hashmap) h, uint64_t key, PMEMoid value, int *err);

/* inserts or updates */
PMEMoid hashmap_put_tm(TOID(struct hashmap) h, uint64_t key, PMEMoid value, int *err);

/* get value or NULL if key is not present */
PMEMoid hashmap_get_tm(TOID(struct hashmap) h, uint64_t key, int *err);

/* delete key from map */
PMEMoid hashmap_delete_tm(TOID(struct hashmap) h, uint64_t key, int *err);

/* boolean return if key is present */
int hashmap_contains_tm(TOID(struct hashmap) h, uint64_t key);

/* foreach function with callback */
int hashmap_foreach(TOID(struct hashmap) h, int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);

int hashmap_foreach_tm(TOID(struct hashmap) h, int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);

/* entirely destroys hashmap and frees memory */
int hashmap_destroy(PMEMobjpool *pop, TOID(struct hashmap) *h);

#ifdef __cplusplus
}
#endif

#endif