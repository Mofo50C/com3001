#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "tm_hashmap.h"
#include "util.h"
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

int hashmap_new(struct hashmap **h)
{
	struct hashmap *map = malloc(sizeof(struct hashmap));
	if (map == NULL)
		goto err_ret;

	*h = map;
	size_t nbuckets = INIT_CAP;
	size_t sz = sizeof(struct buckets) + sizeof(struct entry *) * nbuckets;
	struct buckets *buckets = malloc(sz);
	if (buckets ==  NULL) {
		free(map);
		goto err_ret;
	}

	buckets->num_buckets = nbuckets;
	map->capacity = nbuckets;
	map->buckets = buckets;

	return 0;

err_ret:
	return 1;
}

int hashmap_new_cap(struct hashmap **h, size_t capacity)
{
	struct hashmap *map = malloc(sizeof(struct hashmap));
	if (map == NULL)
		goto err_ret;

	*h = map;
	size_t nbuckets = capacity;
	size_t sz = sizeof(struct buckets) + sizeof(struct entry *) * nbuckets;
	struct buckets *buckets = malloc(sz);
	if (buckets ==  NULL) {
		free(map);
		goto err_ret;
	}

	buckets->num_buckets = nbuckets;
	map->capacity = nbuckets;
	map->buckets = buckets;

	return 0;

err_ret:
	return 1;
}

struct entry *_hashmap_new_entry(uint64_t key, uint64_t hash, void *value)
{
	struct entry *e = STM_NEW(struct entry);
	e->hash = hash;
	e->key = key;
	e->value = value;
	e->next = NULL;

	return e;
}

int _hashmap_resize(struct hashmap *h)
{
	int ret = 0;
	STM_BEGIN() {
		int old_cap = STM_READ(h->capacity);
		int num_grows = STM_READ(h->num_grows);
		if ((STM_READ(h->length) / old_cap) < MAX_LOAD_FACTOR)
			goto end_return;
		
		if (num_grows >= MAX_GROWS) {
			ret = 1;
			goto end_return;
		}

		struct buckets *old_buckets = STM_READ(h->buckets);

		size_t new_cap = TABLE_PRIMES[(num_grows + 1)];
		size_t sz = sizeof(struct buckets) + sizeof(struct entry *) * new_cap;
		struct buckets *new_buckets = STM_ZALLOC(sz);
		new_buckets->num_buckets = new_cap;

		int i;
		for (i = 0; i < old_cap; i++) {
			struct entry *old_head;
			while ((old_head = STM_READ(old_buckets->arr[i])) != NULL) {
				int b = STM_READ(old_head->hash) % new_cap;
				struct entry *new_head = new_buckets->arr[b];
				STM_WRITE(old_buckets->arr[i], STM_READ(old_head->next));
				STM_WRITE(old_head->next, new_head);
				STM_WRITE(new_buckets->arr[b], old_head);
			}
		}

		STM_FREE(old_buckets);
		STM_WRITE(h->buckets, new_buckets);
end_return:	;
	} STM_ONABORT {
		DEBUGABORT();
		ret = -1;
	} STM_END

	return ret;
}


/* retval is optional pointer to capture old value */
/* return = -1: error, 0: inserted, 1: updated */
int hashmap_put_tm(struct hashmap *h, uint64_t key, void *value, void **retval)
{
	uint64_t key_hash = GET_HASH(&key);

	int found = 0;
	int ret = 0;
	STM_BEGIN() {
		int b = key_hash % STM_READ(h->capacity);
		struct buckets *buckets = STM_READ(h->buckets);
		struct entry *entry = STM_READ(buckets->arr[b]);
		if (entry == NULL) {
			struct entry *new_entry = _hashmap_new_entry(key, key_hash, value);
			STM_WRITE(buckets->arr[b], new_entry);
			STM_WRITE(h->length, (STM_READ(h->length) + 1));
		} else {
			struct entry *prev;
			do {
				prev = entry;
				if (STM_READ(entry->key) == key) {
					if (retval)
						*retval = STM_READ(entry->value);
					STM_WRITE(entry->value, value);
					found = 1;
					break;
				}
			} while ((entry = STM_READ(entry->next)) != NULL);

			if (!found) {
				struct entry *new_entry = _hashmap_new_entry(key, key_hash, value);
				STM_WRITE(prev->next, new_entry);
				STM_WRITE(h->length, (STM_READ(h->length) + 1));
			}
		}
	} STM_ONABORT {
		DEBUGABORT();
		ret = -1;
	} STM_END
	
	if (ret) {
		*retval = NULL;
		return ret;
	}
#if defined(HASHMAP_RESIZABLE)
	_hashmap_resize(h);
#endif
	return found;
}

void *hashmap_get_tm(struct hashmap *h, uint64_t key, int *err)
{
	uint64_t key_hash = GET_HASH(&key);
	void *retval = NULL;

	int ret = 0;
	STM_BEGIN() {
		int bucket = key_hash % STM_READ(h->capacity);
		struct buckets *buckets = STM_READ(h->buckets);

		struct entry *entry;
		for (entry = STM_READ(buckets->arr[bucket]); 
			entry != NULL;
			entry = STM_READ(entry->next)) 
		{
			if (STM_READ(entry->key) == key) {
				retval = STM_READ(entry->value);
				break;
			}
		}
	} STM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} STM_END
	
	if (err)
		*err = ret;

	if (ret) return NULL;

	return retval;
}

void *hashmap_delete_tm(struct hashmap *h, uint64_t key, int *err)
{
	uint64_t key_hash = GET_HASH(&key);

	int ret = 0;
	int found = 0;
	void *oldval = NULL;
	STM_BEGIN() {
		DEBUGPRINT("hello!");
		int b = key_hash % STM_READ(h->capacity);
		struct buckets *buckets = STM_READ(h->buckets);
		struct entry *head = STM_READ(buckets->arr[b]);
		if (head == NULL)
			goto end;

		if (STM_READ(head->key) == key) {
			found = 1;
			oldval = STM_READ(head->value);
			STM_WRITE(buckets->arr[b], STM_READ(head->next));
			STM_FREE(head);
			goto end;
		}

		struct entry *prev = head;
		for (head = STM_READ(head->next);
			head != NULL;
			prev = head, head = STM_READ(head->next))
		{
			if (STM_READ(head->key) == key) {
				found = 1;
				break;
			}
		}

		if (found) {
			oldval = STM_READ(head->value);
			STM_WRITE(prev->next, STM_READ(head->next));
			STM_FREE(head);
		}
end:	;
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

	return NULL;
}

int hashmap_contains_tm(struct hashmap *h, uint64_t key)
{
	uint64_t key_hash = GET_HASH(&key);

	int ret = 0;
	STM_BEGIN() {
		int bucket = key_hash % STM_READ(h->capacity);
		struct buckets *buckets = STM_READ(h->buckets);

		struct entry *entry;
		for (entry = STM_READ(buckets->arr[bucket]); 
			entry != NULL;
			entry = STM_READ(entry->next)) 
		{
			if (STM_READ(entry->key) == key) {
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

int hashmap_foreach(struct hashmap *h, int (*cb)(uint64_t key, void *value, void *arg), void *arg)
{
	struct buckets *buckets = h->buckets;
	struct entry *var;

	for (size_t i = 0; i < buckets->num_buckets; i++)  {
		for (var = buckets->arr[i];
			var != NULL; 
			var = var->next) 
		{
			if(cb(var->key, var->value, arg))
				return 1;
		}

	}

	return 0;
}

int hashmap_foreach_tm(struct hashmap *h, int (*cb)(uint64_t key, void *value, void *arg), void *arg)
{
	int err = 0;
	int ret = 0;
	STM_BEGIN() {
		struct buckets *buckets = STM_READ(h->buckets);
		struct entry *var;

		for (size_t i = 0; i < STM_READ(buckets->num_buckets); i++)  {
			for (var = STM_READ(buckets->arr[i]); 
				var != NULL; 
				var = STM_READ(var->next)) 
			{
				ret = cb(STM_READ(var->key), STM_READ(var->value), arg);
				if (ret)
					goto end;
			}
		}
end:	;
	} STM_ONABORT {
		DEBUGABORT();
		err = 1;
	} STM_END

	if (err)
		return -1;
	
	return ret;
}

void _hashmap_destroy_entries(struct hashmap *h)
{
	if (h->length > 0) {
		int i;
		struct entry *head;
		struct buckets *buckets = h->buckets;
		for (i = 0; i < h->capacity; i++) {
			head = buckets->arr[i];
			if (head == NULL) 
				continue;

			struct entry *e;
			while ((e = head->next) != NULL) {
				buckets->arr[i] = e;
				free(head->value);
				free(head);
				head = e;
			}
			free(head);
			buckets->arr[i] = NULL;
		}
	}
}

void hashmap_destroy(struct hashmap **h)
{
	struct hashmap *map = *h;
	_hashmap_destroy_entries(map);

	free(map->buckets);
	map->buckets = NULL;
	free(map);
	*h = NULL;
}

