#include <stdlib.h>
#include "hashmap.h"
#include "fnv.h"

#define MAX_LOAD_FACTOR 0.75
#define MAX_GROWS 8
#define INIT_CAP TABLE_PRIMES[0]

static int TABLE_PRIMES[] = {11, 23, 53, 97, 193, 389, 769, 1543, 3079};

uint64_t _hash(uintptr_t key)
{
	return fnv64((uint64_t)key);
}

hashmap_t *hashmap_new(void)
{
	hashmap_t *h = malloc(sizeof(hashmap_t));
	h->table = calloc(INIT_CAP, sizeof(hashmap_entry_t));
	h->length = 0;
	h->num_grows = 0;
	h->capacity = INIT_CAP;
	
	return h;
}

void hashmap_entry_init(hashmap_entry_t *e, uintptr_t key, uint64_t hash, hashmap_value_t *value)
{
	e->key = key;
	e->hash = hash;
	e->occupied = 1;
	if (value) {
		e->value = *value;
	} else {
		e->value.pval = NULL;
		e->value.size = 0;
	}
}

void hashmap_entry_clear(hashmap_entry_t *e)
{
	e->key = 0;
	e->hash = 0;
	e->occupied = 0;

	hashmap_value_t *v = &e->value;
	free(v->pval);
	v->pval = NULL;
	v->size = 0;
}

int _hashmap_insert(hashmap_t *h, uintptr_t key, uint64_t hash, hashmap_value_t *value, int update)
{
	int slot = hash % h->capacity;
	int ret = 0;
	int i;
	hashmap_entry_t *entry;
	for (i = 0; i < h->capacity; i++) {
		entry = &h->table[slot];
		if (entry->key == key) {
			if (update && value) {
				entry->value = *value;
				ret = 1;
			}
			break;
		}

		if (!entry->occupied) {
			hashmap_entry_init(entry, key, hash, value);
			h->length++;
			ret = 1;
			break;
		}

		slot = (slot + 1) % h->capacity;
	}

	return ret;
}

void hashmap_resize(hashmap_t *h)
{
	if (((float)h->length / h->capacity) < MAX_LOAD_FACTOR) return;

	if (h->num_grows == MAX_GROWS) return;

	int old_cap = h->capacity;
	int new_cap = TABLE_PRIMES[++h->num_grows];
	hashmap_entry_t *old_table = h->table;
	hashmap_entry_t *new_table = calloc(new_cap, sizeof(hashmap_entry_t));

	h->table = new_table;
	h->capacity = new_cap;
	h->length = 0;

	int i;
	hashmap_entry_t *entry;
	for (i = 0; i < old_cap; i++) {
		entry = &old_table[i];
		if (!entry->occupied) continue;
		_hashmap_insert(h, entry->key, entry->hash, &entry->value, 0);
	}

	free(old_table);
}

int hashmap_insert(hashmap_t *h, uintptr_t key, hashmap_value_t *value)
{
	uint64_t key_hash = _hash(key);
	int ret = _hashmap_insert(h, key, key_hash, value, 0);

	if (((float)h->length / h->capacity) >= MAX_LOAD_FACTOR) {
		hashmap_resize(h);
	}

	return ret;
}


int hashmap_put(hashmap_t *h, uintptr_t key, hashmap_value_t *value)
{
	uint64_t key_hash = _hash(key);
	int ret = _hashmap_insert(h, key, key_hash, value, 1);

	if (((float)h->length / h->capacity) >= MAX_LOAD_FACTOR) {
		hashmap_resize(h);
	}

	return ret;
}


hashmap_entry_t *hashmap_get(hashmap_t *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int i;
	hashmap_entry_t *entry;
	for (i = 0; i < h->capacity; i++) {
		entry = &h->table[slot];
		if (!entry->occupied) return NULL;
		if (entry->key == key) return entry;
		slot = (slot + 1) % h->capacity;
	} 

	return NULL;
}

int hashmap_delete(hashmap_t *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int prev_slot, actual_slot, found = 0;
	hashmap_entry_t *entry;
	do {
		entry = &h->table[slot];
		if (!entry->occupied) break;

		if (!found && entry->key == key) {
			hashmap_entry_clear(entry);
			h->length--;
			prev_slot = slot;
			found = 1;
		} else {
			actual_slot = entry->hash % h->capacity;
			if ((slot > prev_slot && (actual_slot <= prev_slot || actual_slot > slot)) || 
			(slot < prev_slot && (actual_slot <= prev_slot && actual_slot > slot))) {
				hashmap_entry_init(&h->table[prev_slot], entry->key, entry->hash, &entry->value);
				hashmap_entry_clear(entry);
				prev_slot = slot;
			}
		}

		slot = (slot + 1) % h->capacity;
	} while (1);

	return found;
}

int hashmap_contains(hashmap_t *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int i;
	hashmap_entry_t *entry;
	for (i = 0; i < h->capacity; i++) {
		entry = &h->table[slot];
		if (!entry->occupied) return 0;
		if (entry->key == key) return 1;
		slot = (slot + 1) % h->capacity;
	}

	return 0;
}

void hashmap_empty(hashmap_t *h)
{
	free(h->table);
	h->table = calloc(INIT_CAP, sizeof(hashmap_entry_t));
	h->capacity = INIT_CAP;
	h->length = 0;
	h->num_grows = 0;
}

void hashmap_destroy(hashmap_t *h)
{
	free(h->table);
	free(h);
}

