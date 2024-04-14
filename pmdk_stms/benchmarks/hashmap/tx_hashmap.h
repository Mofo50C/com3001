#ifndef TX_HASHMAP_H
#define TX_HASHMAP_H 1

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
int hashmap_new(TOID(struct hashmap) *h);

/* inserts or updates */
int hashmap_put(TOID(struct hashmap) h, uint64_t key, PMEMoid value, PMEMoid *retval);

/* get value or NULL if key is not present */
PMEMoid hashmap_get(TOID(struct hashmap) h, uint64_t key, int *err);

/* delete key from map */
PMEMoid hashmap_delete(TOID(struct hashmap) h, uint64_t key, int *err);

/* boolean return if key is present */
int hashmap_contains(TOID(struct hashmap) h, uint64_t key);

/* foreach function with callback */
int hashmap_foreach(TOID(struct hashmap) h, int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);

/* entirely destroys hashmap and frees memory */
int hashmap_destroy(TOID(struct hashmap) *h);

#ifdef __cplusplus
}
#endif

#endif