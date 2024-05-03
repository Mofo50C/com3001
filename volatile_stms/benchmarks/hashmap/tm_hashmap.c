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

#define HASHMAP_RESIZABLE

#define INIT_CAP TABLE_PRIMES[0]

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

int hashmap_new(struct tm_hashmap **h)
{
	return hashmap_new_cap(h, INIT_CAP);
}

int hashmap_new_cap(struct tm_hashmap **h, size_t capacity)
{
	size_t nbuckets;
	int num_grows = -1;

	do {
		nbuckets = TABLE_PRIMES[++num_grows]; 
	} while (nbuckets < capacity);

	struct tm_hashmap *map = malloc(sizeof(struct tm_hashmap));
	if (map == NULL)
		return 1;

	memset(map, 0, sizeof(struct tm_hashmap));

	*h = map;

	size_t sz = sizeof(struct tm_hashmap_buckets) + sizeof(struct tm_hashmap_entry *) * nbuckets;
	struct tm_hashmap_buckets *buckets = malloc(sz);
	if (buckets ==  NULL) {
		free(map);
		return 1;
	}
	memset(buckets, 0, sz);

	buckets->num_buckets = nbuckets;
	map->capacity = nbuckets;
	map->buckets = buckets;
	map->num_grows = num_grows;

	return 0;
}

void hashmap_entry_init(struct tm_hashmap_entry *e, uint64_t key, uint64_t hash, void *value)
{
	e->hash = hash;
	e->key = key;
	e->value = value;
	e->next = NULL;
}

struct tm_hashmap_entry *hashmap_entry_new_tm(uint64_t key, uint64_t hash, void *value)
{
	struct tm_hashmap_entry *e = STM_ZNEW(struct tm_hashmap_entry);
	hashmap_entry_init(e, key, hash, value);
	return e;
}

struct tm_hashmap_entry *hashmap_entry_new(uint64_t key, uint64_t hash, void *value)
{
	struct tm_hashmap_entry *e = malloc(sizeof(struct tm_hashmap_entry));
	if (e == NULL)
		return NULL;

	hashmap_entry_init(e, key, hash, value);
	return e;
}

int hashmap_resize_tm(struct tm_hashmap *h)
{
	int ret = 0;
	STM_BEGIN() {
		int old_cap = STM_READ(h->capacity);
		int num_grows = STM_READ(h->num_grows);
		int length = STM_READ(h->length);
		if (((length / (float)old_cap) < MAX_LOAD_FACTOR) || (num_grows >= MAX_GROWS))
			goto end_return;

		struct tm_hashmap_buckets *old_buckets = STM_READ(h->buckets);

		size_t new_cap = TABLE_PRIMES[++num_grows];
		size_t sz = sizeof(struct tm_hashmap_buckets) + sizeof(struct tm_hashmap_entry *) * new_cap;
		struct tm_hashmap_buckets *new_buckets = STM_ZALLOC(sz);
		new_buckets->num_buckets = new_cap;

		int i;
		for (i = 0; i < old_cap; i++) {
			struct tm_hashmap_entry *old_head;
			while ((old_head = STM_READ(old_buckets->arr[i])) != NULL) {
				int b = STM_READ(old_head->hash) % new_cap;
				STM_WRITE(old_buckets->arr[i], STM_READ(old_head->next));
				STM_WRITE(old_head->next, new_buckets->arr[b]);
				STM_WRITE(new_buckets->arr[b], old_head);
			}
		}

		STM_WRITE(h->num_grows, num_grows);
		STM_WRITE(h->capacity, new_cap);
		STM_WRITE(h->buckets, new_buckets);
		STM_FREE(old_buckets);
end_return:	;
	} STM_ONABORT {
		DEBUGABORT();
		ret = -1;
	} STM_END

	return ret;
}

int hashmap_resize(struct tm_hashmap *h)
{
	int old_cap = h->capacity;
	int num_grows = h->num_grows;
	int length = h->length;

	if (((length / (float)old_cap) < MAX_LOAD_FACTOR) || (num_grows >= MAX_GROWS))
		return 0;

	struct tm_hashmap_buckets *old_buckets = h->buckets;

	size_t new_cap = TABLE_PRIMES[++num_grows];
	size_t sz = sizeof(struct tm_hashmap_buckets) + sizeof(struct tm_hashmap_entry *) * new_cap;
	struct tm_hashmap_buckets *new_buckets = malloc(sz);
	if (new_buckets == NULL)
		return 1;

	memset(new_buckets, 0, sz);
	new_buckets->num_buckets = new_cap;

	int i;
	for (i = 0; i < old_cap; i++) {
		struct tm_hashmap_entry *old_head;
		while ((old_head = old_buckets->arr[i]) != NULL) {
			int b = old_head->hash % new_cap;
			old_buckets->arr[i] = old_head->next;
			old_head->next = new_buckets->arr[b];
			new_buckets->arr[b] = old_head;
		}
	}

	h->num_grows = num_grows;
	h->capacity = new_cap;
	h->buckets = new_buckets;
	free(old_buckets);

	return 0;
}


/* oldval is either NULL or the value before update */
void *hashmap_put(struct tm_hashmap *h, uint64_t key, void *value, int *err)
{
	uint64_t key_hash = GET_HASH(&key);

	void *oldval = NULL;
	int found = 0;

	int b = key_hash % h->capacity;
	struct tm_hashmap_buckets *buckets = h->buckets;
	struct tm_hashmap_entry *entry = buckets->arr[b];
	if (entry == NULL) {
		struct tm_hashmap_entry *new_entry = hashmap_entry_new(key, key_hash, value);
		if (new_entry == NULL)
			goto err_return;

		buckets->arr[b] = new_entry;
		h->length++;
	} else {
		struct tm_hashmap_entry *prev;
		do {
			prev = entry;
			if (entry->key == key) {
				oldval = entry->value;
				entry->value = value;
				found = 1;
				break;
			}
		} while ((entry = entry->next) != NULL);

		if (!found) {
			struct tm_hashmap_entry *new_entry = hashmap_entry_new(key, key_hash, value);
			if (new_entry == NULL)
				goto err_return;

			prev->next = new_entry;
			h->length++;
		}
	}

#if defined(HASHMAP_RESIZABLE)
	hashmap_resize(h);
#endif
	return oldval;

err_return:
	if (err)
		*err = 1;
	
	return NULL;
}

void *hashmap_put_tm(struct tm_hashmap *h, uint64_t key, void *value, int *err)
{
	uint64_t key_hash = GET_HASH(&key);

	void *oldval = NULL;
	int found = 0;
	int ret = 0;
	STM_BEGIN() {
		int b = key_hash % STM_READ(h->capacity);
		struct tm_hashmap_buckets *buckets = STM_READ(h->buckets);
		struct tm_hashmap_entry *entry = STM_READ(buckets->arr[b]);
		if (entry == NULL) {
			struct tm_hashmap_entry *new_entry = hashmap_entry_new_tm(key, key_hash, value);
			STM_WRITE(buckets->arr[b], new_entry);
			STM_WRITE(h->length, (STM_READ(h->length) + 1));
		} else {
			struct tm_hashmap_entry *prev;
			do {
				prev = entry;
				if (STM_READ(entry->key) == key) {
					oldval = STM_READ(entry->value);
					STM_WRITE(entry->value, value);
					found = 1;
					break;
				}
			} while ((entry = STM_READ(entry->next)) != NULL);

			if (!found) {
				struct tm_hashmap_entry *new_entry = hashmap_entry_new_tm(key, key_hash, value);
				STM_WRITE(prev->next, new_entry);
				STM_WRITE(h->length, (STM_READ(h->length) + 1));
			}
		}
	} STM_ONABORT {
		DEBUGABORT();
		ret = 1;
	} STM_END
	
	if (err)
		*err = ret;

	if (ret)
		return NULL;

#if defined(HASHMAP_RESIZABLE)
	hashmap_resize_tm(h);
#endif
	return oldval;
}

void *hashmap_get_tm(struct tm_hashmap *h, uint64_t key, int *err)
{
	uint64_t key_hash = GET_HASH(&key);
	void *retval = NULL;

	int ret = 0;
	STM_BEGIN() {
		int bucket = key_hash % STM_READ(h->capacity);
		struct tm_hashmap_buckets *buckets = STM_READ(h->buckets);

		struct tm_hashmap_entry *entry;
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

void *hashmap_delete_tm(struct tm_hashmap *h, uint64_t key, int *err)
{
	uint64_t key_hash = GET_HASH(&key);

	int ret = 0;
	int found = 0;
	void *oldval = NULL;
	STM_BEGIN() {
		int b = key_hash % STM_READ(h->capacity);
		struct tm_hashmap_buckets *buckets = STM_READ(h->buckets);
		struct tm_hashmap_entry *head = STM_READ(buckets->arr[b]);
		if (head == NULL)
			goto end;

		if (STM_READ(head->key) == key) {
			STM_WRITE(buckets->arr[b], STM_READ(head->next));
			STM_FREE(head);
			found = 1;
			goto end;
		}

		struct tm_hashmap_entry *prev = head;
		struct tm_hashmap_entry *curr = STM_READ(prev->next);

		while(curr != NULL && STM_READ(curr->key) != key) {
			prev = curr;
			curr = STM_READ(curr->next);
		}

		if (curr != NULL && STM_READ(curr->key) == key) {
			STM_WRITE(prev->next, STM_READ(curr->next));
			STM_FREE(curr);
			found = 1;
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

int hashmap_contains_tm(struct tm_hashmap *h, uint64_t key)
{
	uint64_t key_hash = GET_HASH(&key);

	int ret = 0;
	STM_BEGIN() {
		int bucket = key_hash % STM_READ(h->capacity);
		struct tm_hashmap_buckets *buckets = STM_READ(h->buckets);

		struct tm_hashmap_entry *entry;
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

int hashmap_foreach(struct tm_hashmap *h, int (*cb)(uint64_t key, void *value, void *arg), void *arg)
{
	struct tm_hashmap_buckets *buckets = h->buckets;
	struct tm_hashmap_entry *var;

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

int hashmap_foreach_tm(struct tm_hashmap *h, int (*cb)(uint64_t key, void *value, void *arg), void *arg)
{
	int err = 0;
	int ret = 0;
	STM_BEGIN() {
		struct tm_hashmap_buckets *buckets = STM_READ(h->buckets);
		struct tm_hashmap_entry *var;

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

void _hashmap_destroy_entries(struct tm_hashmap *h)
{
	if (h->length > 0) {
		int i;
		struct tm_hashmap_entry *head;
		struct tm_hashmap_buckets *buckets = h->buckets;
		for (i = 0; i < h->capacity; i++) {
			head = buckets->arr[i];
			if (head == NULL) 
				continue;

			struct tm_hashmap_entry *next;
			while (head->next != NULL) {
				next = head->next;
				free(head->value);
				free(head);
				head = next;
			}
			free(head->value);
			free(head);
			buckets->arr[i] = NULL;
		}
	}
}

void hashmap_destroy(struct tm_hashmap **h)
{
	struct tm_hashmap *map = *h;
	if (map == NULL)
		return;

	_hashmap_destroy_entries(map);

	free(map->buckets);
	map->buckets = NULL;
	free(map);
	*h = NULL;
}

