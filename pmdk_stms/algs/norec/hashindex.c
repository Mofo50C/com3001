#include <stdlib.h>
#include <string.h>
#include "hashindex.h"
#include "util.h"

#define MAX_LOAD_FACTOR 0.75
#define MAX_GROWS 8
#define INIT_CAP TABLE_PRIMES[0]

static int TABLE_PRIMES[] = {11, 23, 53, 97, 193, 389, 769, 1543, 3079};

uint64_t _hash(uintptr_t key)
{
	return fnv64((uint64_t)key);
}

hashindex_t *hashindex_new(void)
{
	hashindex_t *h = malloc(sizeof(hashindex_t));
	h->table = calloc(INIT_CAP, sizeof(hashindex_entry_t));
	h->length = 0;
	h->num_grows = 0;
	h->capacity = INIT_CAP;
	
	return h;
}

void hashindex_entry_init(hashindex_entry_t *e, uintptr_t key, uint64_t hash, uint64_t value)
{
	e->key = key;
	e->hash = hash;
	e->occupied = 1;
	e->index = value;
}

void hashindex_entry_clear(hashindex_entry_t *e)
{
	e->key = 0;
	e->hash = 0;
	e->occupied = 0;
	e->index = 0;
}

int _hashindex_insert(hashindex_t *h, uintptr_t key, uint64_t hash, uint64_t value, int update)
{
	int slot = hash % h->capacity;
	int ret = 0;
	int i;
	hashindex_entry_t *entry;
	for (i = 0; i < h->capacity; i++) {
		entry = &h->table[slot];
		if (entry->key == key) {
			if (update) {
				entry->index = value;
				ret = 1;
			}
			break;
		}

		if (!entry->occupied) {
			hashindex_entry_init(entry, key, hash, value);
			h->length++;
			ret = 1;
			break;
		}

		slot = (slot + 1) % h->capacity;
	}

	return ret;
}

void hashindex_resize(hashindex_t *h)
{
	if (((float)h->length / h->capacity) < MAX_LOAD_FACTOR) return;

	if (h->num_grows == MAX_GROWS) return;

	int old_cap = h->capacity;
	int new_cap = TABLE_PRIMES[++h->num_grows];
	hashindex_entry_t *old_table = h->table;
	hashindex_entry_t *new_table = calloc(new_cap, sizeof(hashindex_entry_t));

	h->table = new_table;
	h->capacity = new_cap;
	h->length = 0;

	int i;
	hashindex_entry_t *entry;
	for (i = 0; i < old_cap; i++) {
		entry = &old_table[i];
		if (!entry->occupied) continue;
		_hashindex_insert(h, entry->key, entry->hash, entry->index, 0);
	}

	free(old_table);
}

int hashindex_insert(hashindex_t *h, uintptr_t key, uint64_t value)
{
	uint64_t key_hash = _hash(key);
	int ret = _hashindex_insert(h, key, key_hash, value, 0);

	if (((float)h->length / h->capacity) >= MAX_LOAD_FACTOR) {
		hashindex_resize(h);
	}

	return ret;
}


int hashindex_put(hashindex_t *h, uintptr_t key, uint64_t value)
{
	uint64_t key_hash = _hash(key);
	int ret = _hashindex_insert(h, key, key_hash, value, 1);

	if (((float)h->length / h->capacity) >= MAX_LOAD_FACTOR) {
		hashindex_resize(h);
	}

	return ret;
}


hashindex_entry_t *hashindex_get(hashindex_t *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int i;
	hashindex_entry_t *entry;
	for (i = 0; i < h->capacity; i++) {
		entry = &h->table[slot];
		if (!entry->occupied) return NULL;
		if (entry->key == key) return entry;
		slot = (slot + 1) % h->capacity;
	} 

	return NULL;
}

int hashindex_delete(hashindex_t *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int prev_slot, actual_slot, found = 0;
	hashindex_entry_t *entry;
	do {
		entry = &h->table[slot];
		if (!entry->occupied) break;

		if (!found && entry->key == key) {
			hashindex_entry_clear(entry);
			h->length--;
			prev_slot = slot;
			found = 1;
		} else {
			actual_slot = entry->hash % h->capacity;
			if ((slot > prev_slot && (actual_slot <= prev_slot || actual_slot > slot)) || 
			(slot < prev_slot && (actual_slot <= prev_slot && actual_slot > slot))) {
				hashindex_entry_init(&h->table[prev_slot], entry->key, entry->hash, entry->index);
				hashindex_entry_clear(entry);
				prev_slot = slot;
			}
		}

		slot = (slot + 1) % h->capacity;
	} while (1);

	return found;
}

int hashindex_contains(hashindex_t *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int i;
	hashindex_entry_t *entry;
	for (i = 0; i < h->capacity; i++) {
		entry = &h->table[slot];
		if (!entry->occupied) return 0;
		if (entry->key == key) return 1;
		slot = (slot + 1) % h->capacity;
	}

	return 0;
}

void hashindex_empty(hashindex_t *h)
{
	memset(h->table, 0, h->capacity * sizeof(*h->table));
	h->length = 0;
}

void hashindex_destroy(hashindex_t *h)
{
	free(h->table);
	free(h);
}

