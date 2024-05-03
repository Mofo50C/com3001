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

void hashmap_entry_init(TOID(struct tm_hashmap_entry) e, uint64_t key, uint64_t hash, PMEMoid value)
{
	D_RW(e)->hash = hash;
	D_RW(e)->key = key;
	D_RW(e)->value = value;
	D_RW(e)->next = TOID_NULL(struct tm_hashmap_entry);
}

TOID(struct tm_hashmap_entry) hashmap_entry_new_tm(uint64_t key, uint64_t hash, PMEMoid value)
{
	TOID(struct tm_hashmap_entry) new_entry = PTM_ZNEW(struct tm_hashmap_entry);
	hashmap_entry_init(new_entry, key, hash, value);
	return new_entry;
}

TOID(struct tm_hashmap_entry) hashmap_entry_new(uint64_t key, uint64_t hash, PMEMoid value)
{
	TOID(struct tm_hashmap_entry) new_entry = TX_ZNEW(struct tm_hashmap_entry);
	hashmap_entry_init(new_entry, key, hash, value);
	return new_entry;
}

int hashmap_resize_tm(TOID(struct tm_hashmap) h)
{
	int ret = 0;
	PTM_BEGIN() {
		int old_cap = PTM_READ(D_RW(h)->capacity);
		int num_grows = PTM_READ(D_RW(h)->num_grows);
		int length = PTM_READ(D_RW(h)->length);

		// printf("len: %d cap: %d grows: %d\n", length, old_cap, num_grows);
		if (((length / (float)old_cap) < MAX_LOAD_FACTOR) || (num_grows >= MAX_GROWS))
			goto end;

		// printf("resizing...\n");
		TOID(struct tm_hashmap_buckets) old_buckets = PTM_READ(D_RW(h)->buckets);

		size_t new_cap = TABLE_PRIMES[++num_grows];
		size_t sz = sizeof(struct tm_hashmap_buckets) + sizeof(TOID(struct tm_hashmap_entry)) * new_cap;
		TOID(struct tm_hashmap_buckets) new_buckets = PTM_ZALLOC(struct tm_hashmap_buckets, sz);
		D_RW(new_buckets)->num_buckets = new_cap;

		int i;
		for (i = 0; i < old_cap; i++) {
			TOID(struct tm_hashmap_entry) old_head;
			while (!TOID_IS_NULL((old_head = PTM_READ(D_RW(old_buckets)->arr[i])))) {
				int b = PTM_READ(D_RW(old_head)->hash) % new_cap;
				PTM_WRITE(D_RW(old_buckets)->arr[i], PTM_READ(D_RW(old_head)->next));
				PTM_WRITE(D_RW(old_head)->next, D_RW(new_buckets)->arr[b]);
				PTM_WRITE(D_RW(new_buckets)->arr[b], old_head);
			}
		}

		PTM_WRITE(D_RW(h)->num_grows, num_grows);
		PTM_WRITE(D_RW(h)->capacity, new_cap);
		PTM_WRITE(D_RW(h)->buckets, new_buckets);
		PTM_FREE(old_buckets);
end:	;
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

	// printf("len: %d cap: %d grows: %d\n", length, old_cap, num_grows);
	if (((length / (float)old_cap) < MAX_LOAD_FACTOR) || (num_grows >= MAX_GROWS))
		return 0;

	// printf("resizing...\n");
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


/* returns old value old OID_NULL */
PMEMoid hashmap_put_tm(TOID(struct tm_hashmap) h, uint64_t key, PMEMoid value, int *err)
{
	uint64_t key_hash = GET_HASH(&key);

	PMEMoid oldval = OID_NULL;
	int found = 0;
	int ret = 0;
	PTM_BEGIN() {
		int b = key_hash % PTM_READ(D_RW(h)->capacity);
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ(D_RW(h)->buckets);
		TOID(struct tm_hashmap_entry) entry = PTM_READ(D_RW(buckets)->arr[b]);
		if (TOID_IS_NULL(entry)) {
			TOID(struct tm_hashmap_entry) new_entry = hashmap_entry_new_tm(key, key_hash, value);
			PTM_WRITE(D_RW(buckets)->arr[b], new_entry);
			PTM_WRITE(D_RW(h)->length, (PTM_READ(D_RW(h)->length) + 1));
		} else {
			TOID(struct tm_hashmap_entry) prev;
			do {
				prev = entry;
				if (PTM_READ(D_RW(entry)->key) == key) {
					oldval = PTM_READ(D_RW(entry)->value);
					PTM_WRITE(D_RW(entry)->value, value);
					found = 1;
					break;
				}
			} while (!TOID_IS_NULL((entry = PTM_READ(D_RW(entry)->next))));

			if (!found) {
				TOID(struct tm_hashmap_entry) new_entry = hashmap_entry_new_tm(key, key_hash, value);
				PTM_WRITE(D_RW(prev)->next, new_entry);
				PTM_WRITE(D_RW(h)->length, (PTM_READ(D_RW(h)->length) + 1));
			}
		}
	} PTM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} PTM_END
	
	if (err)
		*err = ret;

	if (ret)
		return OID_NULL;

#if defined(HASHMAP_RESIZABLE)
	hashmap_resize_tm(h);
#endif
	return oldval;
}

PMEMoid hashmap_put(PMEMobjpool *pop, TOID(struct tm_hashmap) h, uint64_t key, PMEMoid value, int *err)
{
	uint64_t key_hash = GET_HASH(&key);

	PMEMoid oldval = OID_NULL;
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
					oldval = D_RW(entry)->value;
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

	if (err)
		*err = ret;

	if (ret)
		return OID_NULL;

#if defined(HASHMAP_RESIZABLE)
	hashmap_resize(pop, h);
#endif
	return oldval;
}

PMEMoid hashmap_get_tm(TOID(struct tm_hashmap) h, uint64_t key, int *err)
{
	uint64_t key_hash = GET_HASH(&key);
	PMEMoid retval = OID_NULL;

	int ret = 0;
	PTM_BEGIN() {
		int bucket = key_hash % PTM_READ(D_RW(h)->capacity);
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ(D_RW(h)->buckets);

		TOID(struct tm_hashmap_entry) entry;
		for (entry = PTM_READ(D_RW(buckets)->arr[bucket]); 
			!TOID_IS_NULL(entry);
			entry = PTM_READ(D_RW(entry)->next)) 
		{
			if (PTM_READ(D_RW(entry)->key) == key) {
				retval = PTM_READ(D_RW(entry)->value);
				break;
			}
		}
	} PTM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} PTM_END
	
	if (err)
		*err = ret;

	if (ret) return OID_NULL;

	return retval;
}

PMEMoid hashmap_delete_tm(TOID(struct tm_hashmap) h, uint64_t key, int *err)
{
	uint64_t key_hash = GET_HASH(&key);

	int ret = 0;
	int found = 0;
	PMEMoid oldval = OID_NULL;
	PTM_BEGIN() {
		int b = key_hash % PTM_READ(D_RW(h)->capacity);
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ(D_RW(h)->buckets);
		TOID(struct tm_hashmap_entry) head = PTM_READ(D_RW(buckets)->arr[b]);
		if (TOID_IS_NULL(head))
			goto end;

		if (PTM_READ(D_RW(head)->key) == key) {
			found = 1;
			oldval = PTM_READ(D_RW(head)->value);
			PTM_WRITE(D_RW(buckets)->arr[b], PTM_READ(D_RW(head)->next));
			PTM_FREE(head);
			goto end;
		}

		TOID(struct tm_hashmap_entry) prev = head;
		for (head = PTM_READ(D_RW(head)->next);
			!TOID_IS_NULL(head);
			prev = head, head = PTM_READ(D_RW(head)->next))
		{
			if (PTM_READ(D_RW(head)->key) == key) {
				found = 1;
				break;
			}
		}

		if (found) {
			oldval = PTM_READ(D_RW(head)->value);
			PTM_WRITE(D_RW(prev)->next, PTM_READ(D_RW(head)->next));
			PTM_FREE(head);
		}
end: 	;
	} PTM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} PTM_END

	if (ret)
		goto pTM_err;

	if (found)
		return oldval;

pTM_err:
	if (err)
		*err = ret;

	return OID_NULL;
}

int hashmap_contains_tm(TOID(struct tm_hashmap) h, uint64_t key)
{
	uint64_t key_hash = GET_HASH(&key);

	int ret = 0;
	PTM_BEGIN() {
		int bucket = key_hash % PTM_READ(D_RW(h)->capacity);
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ(D_RW(h)->buckets);

		TOID(struct tm_hashmap_entry) entry;
		for (entry = PTM_READ(D_RW(buckets)->arr[bucket]); 
			!TOID_IS_NULL(entry);
			entry = PTM_READ(D_RW(entry)->next)) 
		{
			if (PTM_READ(D_RW(entry)->key) == key) {
				ret = 1;
				break;
			}
		}
	} PTM_ONABORT {
		DEBUGABORT();
		ret = -1;
	} PTM_END
	
	return ret;
}

int hashmap_foreach(TOID(struct tm_hashmap) h, int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg)
{
	TOID(struct tm_hashmap_buckets) buckets = D_RO(h)->buckets;
	TOID(struct tm_hashmap_entry) var;

	for (size_t i = 0; i < D_RO(buckets)->num_buckets; i++)  {
		for (var = D_RW(buckets)->arr[i]; 
			!TOID_IS_NULL(var); 
			var = D_RO(var)->next) 
		{
			if (cb(D_RO(var)->key, D_RO(var)->value, arg))
				return 1;
		}

	}
	
	return 0;
}

int hashmap_foreach_tm(TOID(struct tm_hashmap) h, int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg)
{
	int err = 0;
	int ret = 0;
	PTM_BEGIN() {
		TOID(struct tm_hashmap_buckets) buckets = PTM_READ(D_RW(h)->buckets);
		TOID(struct tm_hashmap_entry) var;

		for (size_t i = 0; i < PTM_READ(D_RW(buckets)->num_buckets); i++)  {
			for (var = PTM_READ(D_RW(buckets)->arr[i]); 
				!TOID_IS_NULL(var); 
				var = PTM_READ(D_RW(var)->next)) 
			{
				ret = cb(PTM_READ(D_RW(var)->key), PTM_READ(D_RW(var)->value), arg);
				if (ret)
					goto outer;
			}

		}
outer:	;
	} PTM_ONABORT {
		DEBUGABORT();
		err = 1;
	} PTM_END

	if (err)
		return -1;
	
	return ret;
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
					pmemobj_tx_free(D_RO(head)->value);
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

