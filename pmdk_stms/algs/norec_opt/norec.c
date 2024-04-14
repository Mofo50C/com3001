#include <stdbool.h>
#include <stdio.h>
#include <stdatomic.h>
#include "norec.h"
#include "hashmap.h"
#include "util.h"

struct tx_meta {
	pid_t tid;
	int loc;
	int retry;
	hashmap_t *read_set;
	hashmap_t *write_set;
	hashmap_t *action_set;
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

	hashmap_entry_t *entry;

	if (!(entry = hashmap_get(tx->write_set, key))) return 0;

	memcpy(buf, entry->value.pval, size);
	return 1;
}

void norec_rdset_add(void *pdirect, void *src, size_t size)
{
	struct tx_meta *tx = get_tx_meta();
	uintptr_t key = (uintptr_t)pdirect;

	void *pval = malloc(size);
	memcpy(pval, src, size);
	
	hashmap_entry_t *entry;
	if ((entry = hashmap_get(tx->read_set, key))) {
		free(entry->value.pval);
		entry->value.pval = pval;
	} else {
		struct tx_set_entry v = {
			.type = VALUE_PTR,
			.size = size,
			.pval = pval
		};

		hashmap_put(tx->read_set, key, &v);
	}
}

void norec_tx_add(void *pdirect_obj, size_t obj_size)
{
	struct tx_meta *tx = get_tx_meta();

	uintptr_t obj_key = (uintptr_t)pdirect_obj;

	hashmap_entry_t *entry;
	if (!(entry = hashmap_get(tx->action_set, obj_key))) {
		vector_t *acts = vector_init(8);
	
		struct tx_set_entry v = {
			.acts = acts,
			.type = VALUE_ACTS,
			.size = obj_size
		};

		hashmap_put(tx->action_set, obj_key, &v);
	}
}

void norec_tx_write(void *pdirect_obj, size_t obj_size, void *pdirect_field, size_t field_size, void *pval)
{
	struct tx_meta *tx = get_tx_meta();

	uintptr_t obj_key = (uintptr_t)pdirect_obj;
	uintptr_t field_key = (uintptr_t)pdirect_field;

	hashmap_entry_t *entry;
	struct tx_act act = {
		.size = field_size,
		.pdirect = pdirect_field,
		.pval = pval,
		.type = ACTION_WRITE
	};
	if ((entry = hashmap_get(tx->action_set, obj_key))) {
		struct tx_set_entry *v = &entry->value;
		vector_push_back(v->acts, &act);
	} else {
		vector_t *acts = vector_init(8);
		vector_push_back(acts, &act);
	
		struct tx_set_entry v = {
			.acts = acts,
			.type = VALUE_ACTS,
			.size = obj_size
		};

		hashmap_put(tx->action_set, obj_key, &v);
	}

	if ((entry = hashmap_get(tx->write_set, field_key))) {
		struct tx_set_entry *v = &entry->value;
		free(v->pval);
		v->pval = pval;
	} else {
		struct tx_set_entry v = {
			.pval = pval,
			.type = VALUE_PTR,
			.size = field_size
		};

		hashmap_put(tx->write_set, field_key, &v);
	}
}

void norec_tx_free(void *pdirect_obj, size_t obj_size, void *pdirect_field)
{
	struct tx_meta *tx = get_tx_meta();

	uintptr_t obj_key = (uintptr_t)pdirect_obj;

	hashmap_entry_t *entry;
	struct tx_act act = {
		.pdirect = pdirect_field,
		.pval = NULL,
		.type = ACTION_FREE,
	};
	if ((entry = hashmap_get(tx->action_set, obj_key))) {
		struct tx_set_entry *v = &entry->value;
		vector_push_back(v->acts, &act);
	} else {
		vector_t *acts = vector_init(8);
		vector_push_back(acts, &act);
	
		struct tx_set_entry v = {
			.acts = acts,
			.type = VALUE_ACTS,
			.size = obj_size
		};

		hashmap_put(tx->action_set, obj_key, &v);
	}
}

void norec_validate(void)
{
	struct tx_meta *tx = get_tx_meta();

	int time;
	while (1) {
		do {
			time = glb;
		} while (IS_ODD(time));

		int len = 0;
		int i;
		for (i = 0; i < tx->read_set->capacity; i++) {
			if (len >= tx->read_set->length) break;

			hashmap_entry_t *entry = &tx->read_set->table[i];
			if (!entry->occupied) continue;
						
			printf("[%d] validating...\n", tx->tid);
			if (memcmp(entry->value.pval, (void *)entry->key, entry->value.size))
				norec_tx_abort();

			len++;
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

int norec_tx_begin(PMEMobjpool *pop, jmp_buf env)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();

	if (stage == TX_STAGE_NONE) {
		struct tx_meta *tx = get_tx_meta();

		do {
			tx->loc = glb;
		} while (IS_ODD(tx->loc));
		tx->retry = 0;
		tx->action_set = hashmap_new();
		tx->read_set = hashmap_new();
		tx->write_set = hashmap_new();
		tx->tid = gettid();
	}
	
	return pmemobj_tx_begin(pop, env, TX_PARAM_NONE, TX_PARAM_NONE);
}

void norec_rdset_destroy(void)
{
	struct tx_meta *tx = get_tx_meta();

	hashmap_entry_t *entry;
	int len = 0;
	int i;
	for (i = 0; i < tx->read_set->capacity; i++) {
		if (len >= tx->read_set->length) break;

		entry = &tx->read_set->table[i];
		if (!entry->occupied) continue;

		struct tx_set_entry *v = &entry->value;
		free(v->pval);
	}

	hashmap_destroy(tx->read_set);
}

void norec_wrset_destroy(void)
{
	struct tx_meta *tx = get_tx_meta();
	hashmap_entry_t *entry;
	int len = 0;
	int i;
	for (i = 0; i < tx->action_set->capacity; i++) {
		if (len >= tx->action_set->length) break;

		entry = &tx->action_set->table[i];
		if (!entry->occupied) continue;

		struct tx_set_entry *v = &entry->value;
		if (v->type == VALUE_PTR) {
			free(v->pval);
		} else if (v->type == VALUE_ACTS) {
			vector_t *acts = v->acts;
			int i;
			for (i = 0; i < acts->length; i++) {
				struct tx_act *act = &acts->arr[i];
				free(act->pval);
			}
			vector_destroy(acts);
		}

		len++;
	}

	hashmap_destroy(tx->action_set);
	hashmap_destroy(tx->write_set);
}

void norec_tx_commit(void)
{
	struct tx_meta *tx = get_tx_meta();

	if (tx->action_set->length == 0) {
		return pmemobj_tx_commit();
	}

	while (!atomic_compare_exchange_strong(&glb, &tx->loc, tx->loc + 1)) {
		norec_validate();
	}


	hashmap_entry_t *entry;
	int len = 0;
	int i;
	for (i = 0; i < tx->action_set->capacity; i++) {
		if (len >= tx->action_set->length) break;

		entry = &tx->action_set->table[i];
		if (!entry->occupied) continue;
		
		void *pdirect_obj = (void *)entry->key;
		struct tx_set_entry *v = &entry->value;
		pmemobj_tx_add_range_direct(pdirect_obj, v->size);

		if (v->type == VALUE_PTR) {
			printf("[%d] writing...\n", tx->tid);
			memcpy(pdirect_obj, v->pval, v->size);
		} else if (v->type == VALUE_ACTS) {
			printf("[%d] executing actions...\n", tx->tid);
			vector_t *acts = v->acts;

			int i;
			for (i = 0; i < acts->length; i++) {
				struct tx_act *act = &acts->arr[i];

				if (act->type == ACTION_WRITE) {
					printf("[%d] writing...\n", tx->tid);
					memcpy(act->pdirect, act->pval, act->size);
				} else if (act->type == ACTION_FREE) {
					printf("[%d] freeing...\n", tx->tid);
					PMEMoid *oid_ptr = (PMEMoid *)act->pdirect;
					pmemobj_tx_free(*oid_ptr);
					*oid_ptr = OID_NULL;
				}
			}
		}

		len++;
	}
	printf("[%d] committing...\n", tx->tid);
	pmemobj_tx_commit();
	printf("[%d] after commit\n", tx->tid);

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
	norec_rdset_destroy();
	norec_wrset_destroy();

	return pmemobj_tx_end();
}