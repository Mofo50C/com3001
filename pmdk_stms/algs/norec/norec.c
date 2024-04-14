#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include "norec_base.h"
#include "hashindex.h"
#include "util.h"

#define VEC_INIT 8

struct vec_entry {
	void *pval;
	void *addr;
	size_t size;
};

typedef struct vec {
	size_t length;
	size_t capacity;
	struct vec_entry *entries;
} vec_t;

struct tx_meta {
	pid_t tid;
	PMEMobjpool *pop;
	int loc;
	int retry;
	int level;
	vec_t *read_set;
	hashindex_t *write_set;
	vec_t *write_list;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

/* global lock */
static volatile _Atomic int glb = 0;

int norec_get_retry(void)
{
	return get_tx_meta()->retry;
}

pid_t norec_get_tid(void)
{
	return get_tx_meta()->tid;
}

int norec_wrset_get(void *pdirect, void *buf, size_t size)
{
	struct tx_meta *tx = get_tx_meta();
	uintptr_t key = (uintptr_t)pdirect;

	hashindex_entry_t *entry;

	if (!(entry = hashindex_get(tx->write_set, key))) return 0;
	
	void *pval = tx->write_list->entries[entry->index].pval;
	memcpy(buf, pval, size);
	return 1;
}

vec_t *vector_init(void)
{
	vec_t *v = malloc(sizeof(vec_t));
	v->length = 0;
	v->capacity = VEC_INIT;

	size_t sz = VEC_INIT * sizeof(struct vec_entry);
	v->entries = malloc(sz);
	memset(v->entries, 0, sz);
	return v;
}

void vector_resize(vec_t *vec)
{
	size_t new_cap = vec->capacity * 2;
	size_t sz = new_cap * sizeof(struct vec_entry);
	struct vec_entry *tmp = malloc(sz);
	memset(tmp, 0, sz);
	memcpy(tmp, vec->entries, vec->capacity * sizeof(struct vec_entry));
	free(vec->entries);
	vec->entries = tmp;
	vec->capacity = new_cap;
}

size_t vector_push_back(vec_t *vec, struct vec_entry *entry)
{
	if (vec->length >= vec->capacity)
		vector_resize(vec);

	size_t idx = vec->length++;
	vec->entries[idx] = *entry;
	return idx;
}

void vector_destroy(vec_t *vec)
{
	free(vec->entries);
	free(vec);
}

void vector_free_entries(vec_t *vec)
{
	struct vec_entry *entry;
	int i;
	for (i = 0; i < vec->length; i++) {
		entry = &vec->entries[i];
		free(entry->pval);
		entry->pval = NULL;
	}
}

void vector_empty(vec_t *vec)
{
	vector_free_entries(vec);
	memset(vec->entries, 0, vec->length * sizeof(*vec->entries));
	vec->length = 0;
}


void norec_rdset_add(void *pdirect, void *src, size_t size)
{
	struct tx_meta *tx = get_tx_meta();

	void *pval = malloc(size);
	memcpy(pval, src, size);

	struct vec_entry e = {
		.addr = pdirect,
		.pval = pval,
		.size = size
	};
	vector_push_back(tx->read_set, &e);
}

void norec_tx_write(void *pdirect_field, size_t size, void *pval)
{
	struct tx_meta *tx = get_tx_meta();

	uintptr_t field_key = (uintptr_t)pdirect_field;

	hashindex_entry_t *entry;
	if ((entry = hashindex_get(tx->write_set, field_key))) {
		struct vec_entry *v = &tx->write_list->entries[entry->index];
		free(v->pval);
		v->pval = pval;
		v->size = size;
	} else {
		struct vec_entry e = {
			.pval = pval,
			.size = size,
			.addr = pdirect_field
		};
		size_t idx = vector_push_back(tx->write_list, &e);
		hashindex_put(tx->write_set, field_key, idx);
	}
}

void norec_validate(void)
{
	struct tx_meta *tx = get_tx_meta();

	int time;
	while (1) {
		time = glb;
		if (IS_ODD(time)) continue;

		int i;
		for (i = 0; i < tx->read_set->length; i++) {
			struct vec_entry *e = &tx->read_set->entries[i];

			DEBUGPRINT("[%d] validating...", tx->tid);
			if (memcmp(e->pval, e->addr, e->size))
				return norec_tx_abort();
		}

		if (time == glb) {
			tx->loc = time;
			return;
		}
	}
}

int norec_prevalidate(void) {
	struct tx_meta *tx = get_tx_meta();
	return tx->loc != glb;
}

void norec_tx_abort(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx->retry = 1;
	pmemobj_tx_abort(-1);
}

void norec_thread_enter(PMEMobjpool *pop)
{
	struct tx_meta *tx = get_tx_meta();

	tx->pop = pop;
	tx->tid = gettid();
	tx->read_set = vector_init();
	tx->write_list = vector_init();
	tx->write_set = hashindex_new();
}

void norec_preabort(void)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();
	struct tx_meta *tx = get_tx_meta();

	if (stage == TX_STAGE_ONABORT) {
		tx->level = 0;
	}
}

int norec_tx_begin(jmp_buf env)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();
	struct tx_meta *tx = get_tx_meta();

	if (stage == TX_STAGE_NONE || stage == TX_STAGE_WORK) {
		if (stage == TX_STAGE_NONE) {
			tx->retry = 0;
		} else {
			tx->level++;
		}

		do {
			tx->loc = glb;
		} while (IS_ODD(tx->loc));
	}
	
	return pmemobj_tx_begin(tx->pop, env, TX_PARAM_NONE, TX_PARAM_NONE);
}

void norec_rdset_destroy(void)
{
	struct tx_meta *tx = get_tx_meta();

	vector_free_entries(tx->read_set);
	vector_destroy(tx->read_set);
}

void norec_wrset_destroy(void)
{
	struct tx_meta *tx = get_tx_meta();

	vector_free_entries(tx->write_list);
	vector_destroy(tx->write_list);
	hashindex_destroy(tx->write_set);
}

void norec_empty_logs(void)
{
	struct tx_meta *tx = get_tx_meta();

	vector_empty(tx->read_set);
	vector_empty(tx->write_list);
	hashindex_empty(tx->write_set);
}

void norec_tx_commit(void)
{
	struct tx_meta *tx = get_tx_meta();

	if ((tx->level > 0) || (tx->write_list->length == 0)) {
		return pmemobj_tx_commit();
	}

	while (!atomic_compare_exchange_strong(&glb, &tx->loc, tx->loc + 1)) {
		norec_validate();
	}

	struct vec_entry *entry;
	int i;
	for (i = 0; i < tx->write_list->length; i++) {
		entry = &tx->write_list->entries[i];

		DEBUGPRINT("[%d] writing...", tx->tid);
		pmemobj_tx_add_range_direct(entry->addr, entry->size);
		memcpy(entry->addr, entry->pval, entry->size);

	}

	DEBUGPRINT("[%d] committing...", tx->tid);
	pmemobj_tx_commit();
	DEBUGPRINT("[%d] after commit", tx->tid);

	glb = tx->loc + 2;
}

void norec_tx_process(void)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();

	if (stage == TX_STAGE_WORK) {
		norec_tx_commit();
	} else {
		pmemobj_tx_process();
	}
}

int norec_tx_end(void)
{
	struct tx_meta *tx= get_tx_meta();
	if (tx->level > 0) {
		tx->level--;
	} else {
		norec_empty_logs();
	}

	return pmemobj_tx_end();
}

void norec_thread_exit(void)
{
	norec_rdset_destroy();
	norec_wrset_destroy();
}