#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include "tm_hashmap.h"
#include "xxhash.h"

#define RAII
#include "stm.h"

#define MAX_LOAD_FACTOR 1
#define MAX_GROWS 27

#if defined(HASHMAP_RESIZABLE)
#define INIT_CAP TABLE_PRIMES[0]
#else
#define INIT_CAP TABLE_PRIMES[2]
#endif

#define HASH_SEED 69
#define GET_HASH _hash_int

static int TABLE_PRIMES[] = {11, 23, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 
	196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741};

uint64_t _hash_str(const void *item)
{
	return XXH3_64bits_withSeed(*(char **)item, strlen(*(char **)item), HASH_SEED);
}

uint64_t _hash_int(const void *item)
{
	return XXH3_64bits_withSeed(item, sizeof(uint64_t), HASH_SEED);
}

int hashmap_new(PMEMobjpool *pop, TOID(struct hashmap) *h)
{
	int ret = 0;
	TX_BEGIN(pop) {
		TOID(struct hashmap) map = TX_ZNEW(struct hashmap);
		*h = map;
		size_t nbuckets = INIT_CAP;
		size_t sz = sizeof(struct buckets) + sizeof(TOID(struct entry)) * nbuckets;
		TOID(struct buckets) buckets = TX_ZALLOC(struct buckets, sz);
		D_RW(buckets)->num_buckets = nbuckets;
		D_RW(map)->capacity = nbuckets;
		D_RW(map)->buckets = buckets;
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}

int hashmap_new_cap(PMEMobjpool *pop, TOID(struct hashmap) *h, size_t capacity)
{
	int ret = 0;
	TX_BEGIN(pop) {
		TOID(struct hashmap) map = TX_ZNEW(struct hashmap);
		*h = map;
		size_t nbuckets = capacity;
		size_t sz = sizeof(struct buckets) + sizeof(TOID(struct entry)) * nbuckets;
		TOID(struct buckets) buckets = TX_ZALLOC(struct buckets, sz);
		D_RW(buckets)->num_buckets = nbuckets;
		D_RW(map)->capacity = nbuckets;
		D_RW(map)->buckets = buckets;
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}

TOID(struct entry) _hashmap_new_entry(uint64_t key, uint64_t hash, PMEMoid value)
{
	TOID(struct entry) e = STM_NEW(struct entry);
	D_RW(e)->hash = hash;
	D_RW(e)->key = key;
	D_RW(e)->value = value;
	D_RW(e)->next = TOID_NULL(struct entry);

	return e;
}

int _hashmap_resize(TOID(struct hashmap) h)
{
	int ret = 0;
	STM_BEGIN() {
		int old_cap = STM_READ(D_RW(h)->capacity);
		int num_grows = STM_READ(D_RW(h)->num_grows);
		if (((STM_READ(D_RW(h)->length) / old_cap) >= MAX_LOAD_FACTOR) &&
			(num_grows < MAX_GROWS)) 
		{
			TOID(struct buckets) old_buckets = STM_READ(D_RW(h)->buckets);

			size_t new_cap = TABLE_PRIMES[(num_grows + 1)];
			size_t sz = sizeof(struct buckets) + sizeof(TOID(struct entry)) * new_cap;
			TOID(struct buckets) new_buckets = STM_ZALLOC(struct buckets, sz);
			D_RW(new_buckets)->num_buckets = new_cap;

			int i;
			for (i = 0; i < old_cap; i++) {
				TOID(struct entry) old_head;
				while (!TOID_IS_NULL((old_head = STM_READ(D_RW(old_buckets)->arr[i])))) {
					int b = STM_READ(D_RW(old_head)->hash) % new_cap;
					TOID(struct entry) new_head = D_RW(new_buckets)->arr[b];
					STM_WRITE(D_RW(old_buckets)->arr[i], STM_READ(D_RW(old_head)->next));
					STM_WRITE(D_RW(old_head)->next, new_head);
					STM_WRITE(D_RW(new_buckets)->arr[b], old_head);
				}
			}

			STM_FREE(old_buckets);
			STM_WRITE(D_RW(h)->buckets, new_buckets);
		}
	} STM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} STM_END

	return ret;
}


/* retval is optional pointer to capture old value */
/* return = -1: error, 0: inserted, 1: updated */
int hashmap_put_tm(TOID(struct hashmap) h, uint64_t key, PMEMoid value, PMEMoid *retval)
{
	uint64_t key_hash = GET_HASH(&key);

	int found = 0;
	int ret = 0;
	STM_BEGIN() {
		int b = key_hash % STM_READ(D_RW(h)->capacity);
		TOID(struct buckets) buckets = STM_READ(D_RW(h)->buckets);
		TOID(struct entry) entry = STM_READ(D_RW(buckets)->arr[b]);
		if (TOID_IS_NULL(entry)) {
			TOID(struct entry) new_entry = _hashmap_new_entry(key, key_hash, value);
			STM_WRITE(D_RW(buckets)->arr[b], new_entry);
			STM_WRITE(D_RW(h)->length, (STM_READ(D_RW(h)->length) + 1));
		} else {
			TOID(struct entry) prev;
			do {
				prev = entry;
				if (STM_READ(D_RW(entry)->key) == key) {
					if (retval)
						*retval = STM_READ(D_RW(entry)->value);
					STM_WRITE(D_RW(entry)->value, value);
					found = 1;
					break;
				}
			} while (!TOID_IS_NULL((entry = STM_READ(D_RW(entry)->next))));

			if (!found) {
				TOID(struct entry) new_entry = _hashmap_new_entry(key, key_hash, value);
				STM_WRITE(D_RW(prev)->next, new_entry);
				STM_WRITE(D_RW(h)->length, (STM_READ(D_RW(h)->length) + 1));
			}
		}
	} STM_ONABORT {
		DEBUGABORT();
		ret = -1;
	} STM_END
	
	if (ret) {
		*retval = OID_NULL;
		return ret;
	}
#if defined(HASHMAP_RESIZABLE)
	_hashmap_resize(h);
#endif
	return found;
}

PMEMoid hashmap_get_tm(TOID(struct hashmap) h, uint64_t key, int *err)
{
	uint64_t key_hash = GET_HASH(&key);
	PMEMoid retval = OID_NULL;

	int ret = 0;
	STM_BEGIN() {
		int bucket = key_hash % STM_READ(D_RW(h)->capacity);
		TOID(struct buckets) buckets = STM_READ(D_RW(h)->buckets);

		TOID(struct entry) entry;
		for (entry = STM_READ(D_RW(buckets)->arr[bucket]); 
			!TOID_IS_NULL(entry);
			entry = STM_READ(D_RW(entry)->next)) 
		{
			if (STM_READ(D_RW(entry)->key) == key) {
				retval = STM_READ(D_RW(entry)->value);
				break;
			}
		}
	} STM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} STM_END
	
	if (err)
		*err = ret;

	if (ret) return OID_NULL;

	return retval;
}

PMEMoid hashmap_delete_tm(TOID(struct hashmap) h, uint64_t key, int *err)
{
	uint64_t key_hash = GET_HASH(&key);

	int ret = 0;
	int found = 0;
	PMEMoid oldval = OID_NULL;
	STM_BEGIN() {
		int b = key_hash % STM_READ(D_RW(h)->capacity);
		TOID(struct buckets) buckets = STM_READ(D_RW(h)->buckets);
		TOID(struct entry) head = STM_READ(D_RW(buckets)->arr[b]);
		if (TOID_IS_NULL(head))
			goto end;

		if (STM_READ(D_RW(head)->key) == key) {
			found = 1;
			oldval = STM_READ(D_RW(head)->value);
			STM_WRITE(D_RW(buckets)->arr[b], STM_READ(D_RW(head)->next));
			STM_FREE(head);
			goto end;
		}

		TOID(struct entry) prev = head;
		for (head = STM_READ(D_RW(head)->next);
			!TOID_IS_NULL(head);
			prev = head, head = STM_READ(D_RW(head)->next))
		{
			if (STM_READ(D_RW(head)->key) == key) {
				found = 1;
				break;
			}
		}

		if (found) {
			oldval = STM_READ(D_RW(head)->value);
			STM_WRITE(D_RW(prev)->next, STM_READ(D_RW(head)->next));
			STM_FREE(head);
		}
end: 	;
	} STM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} STM_END

	if (ret)
		goto stm_err;

	if (found)
		return oldval;

stm_err:
	if (err)
		*err = ret;

	return OID_NULL;
}

int hashmap_contains_tm(TOID(struct hashmap) h, uint64_t key)
{
	uint64_t key_hash = GET_HASH(&key);

	int ret = 0;
	STM_BEGIN() {
		int bucket = key_hash % STM_READ(D_RW(h)->capacity);
		TOID(struct buckets) buckets = STM_READ(D_RW(h)->buckets);

		TOID(struct entry) entry;
		for (entry = STM_READ(D_RW(buckets)->arr[bucket]); 
			!TOID_IS_NULL(entry);
			entry = STM_READ(D_RW(entry)->next)) 
		{
			if (STM_READ(D_RW(entry)->key) == key) {
				ret = 1;
				break;
			}
		}
	} STM_ONABORT {
		DEBUGABORT();
		ret = -1;
	} STM_END
	
	return ret;
}

int hashmap_foreach(TOID(struct hashmap) h, int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg)
{
	TOID(struct buckets) buckets = D_RO(h)->buckets;
	TOID(struct entry) var;

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

int hashmap_foreach_tm(TOID(struct hashmap) h, int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg)
{
	int err = 0;
	int ret = 0;
	STM_BEGIN() {
		TOID(struct buckets) buckets = STM_READ(D_RW(h)->buckets);
		TOID(struct entry) var;

		for (size_t i = 0; i < STM_READ(D_RW(buckets)->num_buckets); i++)  {
			for (var = STM_READ(D_RW(buckets)->arr[i]); 
				!TOID_IS_NULL(var); 
				var = STM_READ(D_RW(var)->next)) 
			{
				ret = cb(STM_READ(D_RW(var)->key), STM_READ(D_RW(var)->value), arg);
				if (ret)
					goto outer;
			}

		}
outer:	;
	} STM_ONABORT {
		DEBUGABORT();
		err = 1;
	} STM_END

	if (err)
		return -1;
	
	return ret;
}

int _hashmap_destroy_entries(PMEMobjpool *pop, TOID(struct hashmap) h)
{
	int ret = 0;
	TX_BEGIN(pop) {		
		if (D_RO(h)->length > 0) {
			TOID(struct buckets) buckets = D_RO(h)->buckets;
			int i;
			TOID(struct entry) head;
			for (i = 0; i < D_RO(h)->capacity; i++) {
				head = D_RO(buckets)->arr[i];
				if (TOID_IS_NULL(head)) continue;

				TOID(struct entry) e;
				while (!TOID_IS_NULL((e = D_RO(head)->next))) {
					D_RW(buckets)->arr[i] = e;
					pmemobj_tx_free(D_RO(head)->value);
					TX_FREE(head);
					head = e;
				}
				TX_FREE(head);
				D_RW(buckets)->arr[i] = TOID_NULL(struct entry);
			}
		}
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}

int hashmap_destroy(PMEMobjpool *pop, TOID(struct hashmap) *h)
{
	TOID(struct hashmap) map = *h;
	int ret = _hashmap_destroy_entries(pop, map);

	if (ret) return ret;

	TX_BEGIN(pop) {
		TX_FREE(D_RO(map)->buckets);
		D_RW(map)->buckets = TOID_NULL(struct buckets);
		TX_FREE(map);
		*h = TOID_NULL(struct hashmap);
	} TX_ONABORT {
		DEBUGABORT();
		ret = 1;
	} TX_END

	return ret;
}

