#ifndef TM_HASHMAP_H
#define TM_HASHMAP_H 1

#include <stdint.h>
#include <libpmemobj.h>

#define TX_HASHMAP_TYPE_NUM 1001

#ifdef __cplusplus
extern "C" {
#endif

TOID_DECLARE(struct hashmap, TX_HASHMAP_TYPE_NUM);
TOID_DECLARE(struct entry, TX_HASHMAP_TYPE_NUM + 1);
TOID_DECLARE(struct buckets, TX_HASHMAP_TYPE_NUM + 2);

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

int hashmap_new_cap(PMEMobjpool *pop, TOID(struct hashmap) *h, size_t capacity);

/* inserts or updates */
int hashmap_put_tm(TOID(struct hashmap) h, uint64_t key, PMEMoid value, PMEMoid *retval);

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