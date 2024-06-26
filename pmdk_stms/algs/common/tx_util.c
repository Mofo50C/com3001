#include <stdlib.h>
#include <string.h>
#include "tx_util.h"
#include "util.h"

#define VEC_INIT 8

int tx_vector_init(struct tx_vec **vecp)
{
	struct tx_vec *v = malloc(sizeof(struct tx_vec));
	if (v == NULL)
		return 1;

	v->length = 0;
	v->capacity = VEC_INIT;

	size_t sz = VEC_INIT * sizeof(tx_vec_entry_t);
	v->arr = malloc(sz);
	if (v->arr == NULL) {
		free(v);
		return 1;
	}

	memset(v->arr, 0, sz);
	*vecp = v;
	return 0;
}

int tx_vector_resize(struct tx_vec *vec)
{
	size_t new_cap = vec->capacity * 2;
	size_t sz = new_cap * sizeof(tx_vec_entry_t);
	tx_vec_entry_t *tmp = malloc(sz);
	if (tmp == NULL)
		return 1;

	memset(tmp, 0, sz);
	memcpy(tmp, vec->arr, vec->capacity * sizeof(tx_vec_entry_t));
	free(vec->arr);
	vec->arr = tmp;
	vec->capacity = new_cap;

	return 0;
}

/* returns 1 on error */
int tx_vector_append(struct tx_vec *vec, tx_vec_entry_t *entry, size_t *retval)
{
	size_t idx = vec->length++;
	vec->arr[idx] = *entry;
	
	if (retval)
		*retval = idx;

	if (vec->length >= vec->capacity && tx_vector_resize(vec))
		return 1;
 
	return 0;
}


void tx_vector_destroy(struct tx_vec **vecp)
{
	if (*vecp == NULL)
		return;

	struct tx_vec *vec = *vecp;
	free(vec->arr);
	free(vec);
	*vecp = NULL;
}

void _vector_free_entries(struct tx_vec *vec)
{
	tx_vec_entry_t *entry;
	int i;
	for (i = 0; i < vec->length; i++) {
		entry = &vec->arr[i];
		if (entry->pval)
			free(entry->pval);
		entry->pval = NULL;
	}
}

void tx_vector_empty(struct tx_vec *vec)
{
	_vector_free_entries(vec);
	memset(vec->arr, 0, vec->capacity * sizeof(void *));
	vec->length = 0;
}

void tx_vector_clear(struct tx_vec *vec)
{
	memset(vec->arr, 0, vec->capacity * sizeof(void *));
	vec->length = 0;
}

#define MAX_LOAD_FACTOR 0.75
#define MAX_GROWS 8
#define INIT_CAP TABLE_PRIMES[0]

static size_t TABLE_PRIMES[] = {11, 23, 53, 97, 193, 389, 769, 1543, 3079};

uint64_t _hash(uintptr_t key)
{
	return fnv64((uint64_t)key);
}

/* returns 1 on error */
int tx_hash_new(struct tx_hash **hashp)
{
	struct tx_hash *h = malloc(sizeof(struct tx_hash));
	if (h == NULL)
		return 1;

	h->table = calloc(INIT_CAP, sizeof(struct tx_hash_entry));
	if (h->table == NULL) {
		free(h);
		return 1;
	}

	h->length = 0;
	h->num_grows = 0;
	h->capacity = INIT_CAP;
	
	*hashp = h;
	return 0;
}

void tx_hash_entry_init(struct tx_hash_entry *e, uintptr_t key, uint64_t hash, uint64_t value)
{
	e->key = key;
	e->hash = hash;
	e->occupied = 1;
	e->index = value;
}

void tx_hash_entry_clear(struct tx_hash_entry *e)
{
	e->key = 0;
	e->hash = 0;
	e->occupied = 0;
	e->index = 0;
}

int _tx_hash_insert(struct tx_hash *h, uintptr_t key, uint64_t hash, uint64_t value, int update)
{
	int slot = hash % h->capacity;
	int ret = 0;
	int i;
	struct tx_hash_entry *entry;
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
			tx_hash_entry_init(entry, key, hash, value);
			h->length++;
			ret = 1;
			break;
		}

		slot = (slot + 1) % h->capacity;
	}

	return ret;
}

/* returns 1 on error */
int tx_hash_resize(struct tx_hash *h)
{
	if ((((float)h->length / h->capacity) < MAX_LOAD_FACTOR) || (h->num_grows >= MAX_GROWS))
		return 0;

	int old_cap = h->capacity;
	int new_cap = TABLE_PRIMES[++h->num_grows];
	struct tx_hash_entry *old_table = h->table;
	struct tx_hash_entry *new_table = calloc(new_cap, sizeof(struct tx_hash_entry));
	if (new_table == NULL) {
		return 1;
	}

	h->table = new_table;
	h->capacity = new_cap;
	h->length = 0;

	int i;
	struct tx_hash_entry *entry;
	for (i = 0; i < old_cap; i++) {
		entry = &old_table[i];
		if (!entry->occupied) continue;
		_tx_hash_insert(h, entry->key, entry->hash, entry->index, 0);
	}

	free(old_table);

	return 0;
}


/* returns -1 on error */
int tx_hash_insert(struct tx_hash *h, uintptr_t key, uint64_t value)
{
	uint64_t key_hash = _hash(key);
	int err = 0;
	int ret = _tx_hash_insert(h, key, key_hash, value, 0);

	if (((float)h->length / h->capacity) >= MAX_LOAD_FACTOR) {
		err = tx_hash_resize(h);
	}

	if (err)
		return -1;

	return ret;
}

/* returns -1 on error */
int tx_hash_put(struct tx_hash *h, uintptr_t key, uint64_t value)
{
	uint64_t key_hash = _hash(key);
	int err = 0;
	int ret = _tx_hash_insert(h, key, key_hash, value, 1);

	if (((float)h->length / h->capacity) >= MAX_LOAD_FACTOR) {
		err = tx_hash_resize(h);
	}

	if (err)
		return -1;

	return ret;
}


struct tx_hash_entry *tx_hash_get(struct tx_hash *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int i;
	struct tx_hash_entry *entry;
	for (i = 0; i < h->capacity; i++) {
		entry = &h->table[slot];
		if (!entry->occupied) return NULL;
		if (entry->key == key) return entry;
		slot = (slot + 1) % h->capacity;
	} 

	return NULL;
}

int tx_hash_delete(struct tx_hash *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int prev_slot, actual_slot, found = 0;
	struct tx_hash_entry *entry;
	do {
		entry = &h->table[slot];
		if (!entry->occupied) break;

		if (!found && entry->key == key) {
			tx_hash_entry_clear(entry);
			h->length--;
			prev_slot = slot;
			found = 1;
		} else {
			actual_slot = entry->hash % h->capacity;
			if ((slot > prev_slot && (actual_slot <= prev_slot || actual_slot > slot)) || 
			(slot < prev_slot && (actual_slot <= prev_slot && actual_slot > slot))) {
				tx_hash_entry_init(&h->table[prev_slot], entry->key, entry->hash, entry->index);
				tx_hash_entry_clear(entry);
				prev_slot = slot;
			}
		}

		slot = (slot + 1) % h->capacity;
	} while (1);

	return found;
}

int tx_hash_contains(struct tx_hash *h, uintptr_t key)
{
	uint64_t key_hash = _hash(key);

	int slot = key_hash % h->capacity;
	int i;
	struct tx_hash_entry *entry;
	for (i = 0; i < h->capacity; i++) {
		entry = &h->table[slot];
		if (!entry->occupied) return 0;
		if (entry->key == key) return 1;
		slot = (slot + 1) % h->capacity;
	}

	return 0;
}

void tx_hash_empty(struct tx_hash *h)
{
	memset(h->table, 0, h->capacity * sizeof(*h->table));
	h->length = 0;
}

void tx_hash_destroy(struct tx_hash **hashp)
{
	if (*hashp == NULL)
		return;

	struct tx_hash *h = *hashp;
	free(h->table);
	free(h);
	*hashp = NULL;
}
