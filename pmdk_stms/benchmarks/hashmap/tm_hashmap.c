#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include "tm_hashmap.h"
#include "xxhash.h"

#define RAII
#include "ptm.h"

#define MAX_LOAD_FACTOR 1
#define MAX_GROWS 27

#define HASHMAP_RESIZABLE

#define INIT_CAP TABLE_PRIMES[0]

#define HASH_SEED 69
#define GET_HASH _hash_int

static size_t TABLE_PRIMES[] = {11, 23, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 
	196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741};

uint64_t _hash_str(const void *item)
{
	return XXH3_64bits_withSeed(*(char **)item, strlen(*(char **)item), HASH_SEED);
}

uint64_t _hash_int(const void *item)
{
	return XXH3_64bits_withSeed(item, sizeof(uint64_t), HASH_SEED);
}

int hashmap_new_cap(PMEMobjpool *pop, TOID(struct tm_hashmap) *h, size_t capacity)
{
	size_t nbuckets;
	int num_grows = -1;

	do {
		nbuckets = TABLE_PRIMES[++num_grows]; 
	} while (nbuckets < capacity);

	int ret = 0;
	TX_BEGIN(pop) {
		TOID(struct tm_hashmap) map = TX_ZNEW(struct tm_hashmap);
		size_t sz = sizeof(struct tm_hashmap_buckets) + sizeof(TOID(struct tm_hashmap_entry)) * nbuckets;
		TOID(struct tm_hashmap_buckets) buckets = TX_ZALLOC(struct tm_hashmap_buckets, sz);
		D_RW(buckets)->num_buckets = nbuckets;
		D_RW(map)->num_grows = num_grows;
		D_RW(map)->capacity = nbuckets;
		D_RW(map)->buckets = buckets;
		D_RW(map)->length = 0;
		TX_ADD_DIRECT(h);
		*h = map;
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}

int hashmap_new(PMEMobjpool *pop, TOID(struct tm_hashmap) *h)
{
	return hashmap_new_cap(pop, h, INIT_CAP);
}

void hashmap_entry_init(TOID(struct tm_hashmap_entry) e, uint64_t key, uint64_t hash, int value)
{
	D_RW(e)->hash = hash;
	D_RW(e)->key = key;
	D_RW(e)->value = value;
	D_RW(e)->next = TOID_NULL(struct tm_hashmap_entry);
}

TOID(struct tm_hashmap_entry) hashmap_entry_new_tm(uint64_t key, uint64_t hash, int value)
{
	TOID(struct tm_hashmap_entry) new_entry = PTM_ZNEW(struct tm_hashmap_entry);
	hashmap_entry_init(new_entry, key, hash, value);
	return new_entry;
}

TOID(struct tm_hashmap_entry) hashmap_entry_new(uint64_t key, uint64_t hash, int value)
{
	TOID(struct tm_hashmap_entry) new_entry = TX_ZNEW(struct tm_hashmap_entry);
	hashmap_entry_init(new_entry, key, hash, value);
	return new_entry;
}

/* returns -1 on error, 1 if updated and 0 if inserted new */
int hashmap_put_tm(TOID(struct tm_hashmap) h, uint64_t key, int value, int *retval)
{
	uint64_t key_hash = GET_HASH(&key);

	int found = 0;
	int ret = 0;
	PTM_BEGIN() {
		int b = key_hash % PTM_READ_FIELD(h, capacity);
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ_FIELD(h, buckets);
		TOID(struct tm_hashmap_entry) entry = PTM_READ_FIELD(buckets, arr[b]);
		if (TOID_IS_NULL(entry)) {
			TOID(struct tm_hashmap_entry) new_entry = hashmap_entry_new_tm(key, key_hash, value);
			PTM_WRITE_FIELD(buckets, arr[b], new_entry);
			size_t len = PTM_READ_FIELD(h, length) + 1;
			PTM_WRITE_FIELD(h, length, len);
		} else {
			if (PTM_READ_FIELD(entry, key) == key) {
				if (retval)
					*retval = PTM_READ_FIELD(entry, value);
				PTM_WRITE_FIELD(entry, value, value);
				found = 1;
				PTM_RETURN();
			}

			TOID(struct tm_hashmap_entry) prev = entry;
			TOID(struct tm_hashmap_entry) curr = PTM_READ_FIELD(entry, next);
			while (!TOID_IS_NULL(curr)) {
				if (PTM_READ_FIELD(curr, key) == key) {
					if (retval)
						*retval = PTM_READ_FIELD(curr, value);
					PTM_WRITE_FIELD(curr, value, value);
					found = 1;
					PTM_RETURN();
				}
				prev = curr;
				curr = PTM_READ_FIELD(prev, next);
			}

			TOID(struct tm_hashmap_entry) new_entry = hashmap_entry_new_tm(key, key_hash, value);
			PTM_WRITE_FIELD(prev, next, new_entry);
			size_t len = PTM_READ_FIELD(h, length) + 1;
			PTM_WRITE_FIELD(h, length, len);
		}
	} PTM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} PTM_END

	if (ret)
		return -1;

#if defined(HASHMAP_RESIZABLE)
	hashmap_resize_tm(h);
#endif

	return found;
}

int hashmap_put(PMEMobjpool *pop, TOID(struct tm_hashmap) h, uint64_t key, int value, int *oldval)
{
	uint64_t key_hash = GET_HASH(&key);

	int found = 0;
	int ret = 0;
	int b = key_hash % D_RW(h)->capacity;
	TOID(struct tm_hashmap_buckets) buckets = D_RW(h)->buckets;
	TOID(struct tm_hashmap_entry) entry = D_RW(buckets)->arr[b];

	TX_BEGIN(pop) {
		if (TOID_IS_NULL(entry)) {
			TOID(struct tm_hashmap_entry) new_entry = hashmap_entry_new(key, key_hash, value);
			TX_ADD_FIELD(buckets, arr[b]);
			D_RW(buckets)->arr[b] = new_entry;
			TX_ADD_FIELD(h, length);
			D_RW(h)->length++;
		} else {
			TOID(struct tm_hashmap_entry) prev;
			do {
				prev = entry;
				if (D_RW(entry)->key == key) {
					if (oldval)
						*oldval = D_RW(entry)->value;
					TX_ADD_FIELD(entry, value);
					D_RW(entry)->value = value;
					found = 1;
					break;
				}
			} while (!TOID_IS_NULL((entry = D_RW(entry)->next)));

			if (!found) {
				TOID(struct tm_hashmap_entry) new_entry = hashmap_entry_new(key, key_hash, value);
				TX_ADD_FIELD(prev, next);
				D_RW(prev)->next = new_entry;
				TX_ADD_FIELD(h, length);
				D_RW(h)->length++;
			}
		}
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	if (ret)
		return -1;

#if defined(HASHMAP_RESIZABLE)
	hashmap_resize(pop, h);
#endif
	return found;
}

int hashmap_get_tm(TOID(struct tm_hashmap) h, uint64_t key, int *retval)
{
	uint64_t key_hash = GET_HASH(&key);

	int found = 0;
	int ret = 0;
	PTM_BEGIN() {
		int bucket = key_hash % PTM_READ_FIELD(h, capacity);
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ_FIELD(h, buckets);

		TOID(struct tm_hashmap_entry) entry;
		for (entry = PTM_READ_FIELD(buckets, arr[bucket]);
			!TOID_IS_NULL(entry);
			entry = PTM_READ_FIELD(entry, next)) 
		{
			if (PTM_READ_FIELD(entry, key) == key) {
				found = 1;
				if (retval)
					*retval = PTM_READ_FIELD(entry, value);
				break;
			}
		}
	} PTM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} PTM_END
	
	if (ret || !found)
		return 1;

	return 0;
}

int hashmap_delete_tm(TOID(struct tm_hashmap) h, uint64_t key, int *retval)
{
	uint64_t key_hash = GET_HASH(&key);

	int found = 0;
	int ret = 0;
	PTM_BEGIN() {
		int b = key_hash % PTM_READ_FIELD(h, capacity);
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ_FIELD(h, buckets);
		TOID(struct tm_hashmap_entry) head = PTM_READ_FIELD(buckets, arr[b]);
		if (TOID_IS_NULL(head))
			PTM_RETURN();

		if (PTM_READ_FIELD(head, key) == key) {
			TOID(struct tm_hashmap_entry) next = PTM_READ_FIELD(head, next);
			PTM_WRITE_FIELD(buckets, arr[b], next);
			if (retval)
				*retval = PTM_READ_FIELD(head, value);
			PTM_FREE(head);
			size_t len = PTM_READ_FIELD(h, length) - 1;
			PTM_WRITE_FIELD(h, length, len);
			found = 1;
			PTM_RETURN();
		}

		TOID(struct tm_hashmap_entry) prev = head;
		TOID(struct tm_hashmap_entry) curr = PTM_READ_FIELD(prev, next);

		while(!TOID_IS_NULL(curr)) {
			if (PTM_READ_FIELD(curr, key) == key) {
				TOID(struct tm_hashmap_entry) next = PTM_READ_FIELD(curr, next);
				PTM_WRITE_FIELD(prev, next, next);
				if (retval)
					*retval = PTM_READ_FIELD(curr, value);
				PTM_FREE(curr);
				size_t len = PTM_READ_FIELD(h, length) - 1;
				PTM_WRITE_FIELD(h, length, len);
				found = 1;
				PTM_RETURN();
			}
			prev = curr;
			curr = PTM_READ_FIELD(curr, next);
		}
	} PTM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} PTM_END

	if (ret || !found)
		return 1;

	return 0;
}

int hashmap_contains_tm(TOID(struct tm_hashmap) h, uint64_t key)
{
	uint64_t key_hash = GET_HASH(&key);

	int found = 0;
	int ret = 0;
	PTM_BEGIN() {
		int bucket = key_hash % PTM_READ_FIELD(h, capacity);
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ_FIELD(h, buckets);

		TOID(struct tm_hashmap_entry) entry;
		for (entry = PTM_READ_FIELD(buckets, arr[bucket]); 
			!TOID_IS_NULL(entry);
			entry = PTM_READ_FIELD(entry, next)) 
		{
			if (PTM_READ_FIELD(entry, key) == key) {
				found = 1;
				break;
			}
		}
	} PTM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} PTM_END

	if (ret || !found)
		return 1;

	return 0;
}

int hashmap_foreach(TOID(struct tm_hashmap) h, int (*cb)(uint64_t key, int value, void *arg), void *arg)
{
	TOID(struct tm_hashmap_buckets) buckets = D_RO(h)->buckets;
	TOID(struct tm_hashmap_entry) var;

	for (size_t i = 0; i < D_RO(buckets)->num_buckets; i++)  {
		for (var = D_RW(buckets)->arr[i]; 
			!TOID_IS_NULL(var); 
			var = D_RO(var)->next) 
		{
			int ret = cb(D_RO(var)->key, D_RO(var)->value, arg);
			if (ret)
				return ret;
		}
	}
	
	return 0;
}

int _hashmap_destroy_entries(PMEMobjpool *pop, TOID(struct tm_hashmap) h)
{
	int ret = 0;
	TX_BEGIN(pop) {		
		if (D_RO(h)->length > 0) {
			TOID(struct tm_hashmap_buckets) buckets = D_RO(h)->buckets;
			int i;
			TOID(struct tm_hashmap_entry) head;
			for (i = 0; i < D_RO(h)->capacity; i++) {
				head = D_RO(buckets)->arr[i];
				if (TOID_IS_NULL(head)) continue;

				TOID(struct tm_hashmap_entry) e;
				while (!TOID_IS_NULL((e = D_RO(head)->next))) {
					D_RW(buckets)->arr[i] = e;
					TX_FREE(head);
					head = e;
				}
				TX_FREE(head);
				D_RW(buckets)->arr[i] = TOID_NULL(struct tm_hashmap_entry);
			}
		}
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}

int hashmap_destroy(PMEMobjpool *pop, TOID(struct tm_hashmap) *h)
{
	TOID(struct tm_hashmap) map = *h;
	int ret = _hashmap_destroy_entries(pop, map);

	if (ret) return ret;

	TX_BEGIN(pop) {
		TX_FREE(D_RO(map)->buckets);
		D_RW(map)->buckets = TOID_NULL(struct tm_hashmap_buckets);
		TX_FREE(map);
		*h = TOID_NULL(struct tm_hashmap);
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}

int hashmap_resize_tm(TOID(struct tm_hashmap) h)
{
	int ret = 0;
	PTM_BEGIN() {
		int old_cap = PTM_READ(D_RW(h)->capacity);
		int num_grows = PTM_READ(D_RW(h)->num_grows);
		int length = PTM_READ(D_RW(h)->length);
		if (((length / (float)old_cap) < MAX_LOAD_FACTOR) || (num_grows >= MAX_GROWS))
			PTM_RETURN();

		TOID(struct tm_hashmap_buckets) old_buckets = PTM_READ(D_RW(h)->buckets);

		size_t new_cap = TABLE_PRIMES[++num_grows];
		size_t sz = sizeof(struct tm_hashmap_buckets) + sizeof(TOID(struct tm_hashmap_entry)) * new_cap;
		TOID(struct tm_hashmap_buckets) new_buckets = PTM_ZALLOC(struct tm_hashmap_buckets, sz);
		D_RW(new_buckets)->num_buckets = new_cap;

		int i;
		for (i = 0; i < old_cap; i++) {
			TOID(struct tm_hashmap_entry) old_head = PTM_READ_FIELD(old_buckets, arr[i]);
			if (TOID_IS_NULL(old_head))
				continue;

			TOID(struct tm_hashmap_entry) temp;
			while (!TOID_IS_NULL(old_head)) {
				temp = PTM_READ_FIELD(old_head, next);
				int b = PTM_READ_FIELD(old_head, hash) % new_cap;
				TOID(struct tm_hashmap_entry) new_head = D_RO(new_buckets)->arr[b];
				PTM_WRITE_FIELD(old_head, next, new_head);
				D_RW(new_buckets)->arr[b] = old_head;
				old_head = temp;
			}
		}

		PTM_WRITE(D_RW(h)->num_grows, num_grows);
		PTM_WRITE(D_RW(h)->capacity, new_cap);
		PTM_WRITE(D_RW(h)->buckets, new_buckets);
		PTM_FREE(old_buckets);
	} PTM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} PTM_END

	return ret;
}

int hashmap_resize(PMEMobjpool *pop, TOID(struct tm_hashmap) h)
{
	int old_cap = D_RO(h)->capacity;
	int num_grows = D_RO(h)->num_grows;
	int length = D_RO(h)->length;

	if (((length / (float)old_cap) < MAX_LOAD_FACTOR) || (num_grows >= MAX_GROWS))
		return 0;

	TOID(struct tm_hashmap_buckets) old_buckets = D_RO(h)->buckets;
	size_t new_cap = TABLE_PRIMES[++num_grows];
	size_t sz = sizeof(struct tm_hashmap_buckets) + sizeof(TOID(struct tm_hashmap_entry)) * new_cap;
	size_t sz_old = sizeof(struct tm_hashmap_buckets) + sizeof(TOID(struct tm_hashmap_entry)) * old_cap;

	int ret = 0;
	TX_BEGIN(pop) {
		TX_ADD(h);
		TOID(struct tm_hashmap_buckets) new_buckets = TX_ZALLOC(struct tm_hashmap_buckets, sz);
		D_RW(new_buckets)->num_buckets = new_cap;

		pmemobj_tx_add_range(old_buckets.oid, 0, sz_old);

		size_t i;
		for (i = 0; i < old_cap; i++) {
			TOID(struct tm_hashmap_entry) old_head;
			while (!TOID_IS_NULL((old_head = D_RO(old_buckets)->arr[i]))) {
				int b = D_RO(old_head)->hash % new_cap;
				D_RW(old_buckets)->arr[i] = D_RO(old_head)->next;
	
				TX_ADD_FIELD(old_head, next);
				D_RW(old_head)->next = D_RO(new_buckets)->arr[b];
				D_RW(new_buckets)->arr[b] = old_head;
			}
		}
		D_RW(h)->num_grows = num_grows;
		D_RW(h)->capacity = new_cap;
		D_RW(h)->buckets = new_buckets;
		TX_FREE(old_buckets);
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}