#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "tx.h"
#include "norec_base.h"
#include "tx_util.h"
#include "util.h"

struct tx_meta {
	uint64_t loc;
	int irrevoc;
	struct tx_vec *read_set;
	struct tx_vec *write_set;
	struct tx_hash *wrset_lookup;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

/* global lock */
static uint64_t glb = 0;

int norec_isirrevoc(void)
{
	return get_tx_meta()->irrevoc;
}

void norec_tx_restart(void)
{
	return tx_restart();
}

void norec_tx_abort(int err)
{
	return pmemobj_tx_abort(err);
}

bool norec_wrset_get(void *pdirect, void *buf, size_t size)
{
	struct tx_meta *tx = get_tx_meta();
	uintptr_t key = (uintptr_t)pdirect;

	struct tx_hash_entry *entry = tx_hash_get(tx->wrset_lookup, key);
	if (entry == NULL)
		return false;
	
	void *pval = tx->write_set->arr[entry->index].pval;
	memcpy(buf, pval, size);
	return true;
}

void norec_rdset_add(void *pdirect, void *src, size_t size)
{
	struct tx_meta *tx = get_tx_meta();

	int err = 0;
	void *pval = malloc(size);
	if (pval == NULL) {
		err = errno;
		goto err_abort;
	}

	memcpy(pval, src, size);

	struct tx_vec_entry e = {
		.addr = pdirect,
		.pval = pval,
		.size = size
	};

	if (tx_vector_append(tx->read_set, &e, NULL)) {
		err = errno;
		free(pval);
		goto err_abort;
	}
	
	return;

err_abort:
	norec_tx_abort(err);
}

void norec_tx_write(void *pdirect_field, size_t size, void *buf)
{
	struct tx_meta *tx = get_tx_meta();

	int err = 0;
	void *pval = malloc(size);
	if (pval == NULL) {
		err = errno;
		goto err_abort;
	}

	memcpy(pval, buf, size);

	uintptr_t field_key = (uintptr_t)pdirect_field;
	struct tx_hash_entry *entry = tx_hash_get(tx->wrset_lookup, field_key);
	if (entry != NULL) {
		struct tx_vec_entry *v = &tx->write_set->arr[entry->index];
		free(v->pval);
		v->pval = pval;
		v->size = size;
	} else {
		struct tx_vec_entry e = {
			.pval = pval,
			.size = size,
			.addr = pdirect_field
		};

		size_t prev_idx;
		if (tx_vector_append(tx->write_set, &e, &prev_idx)) {
			err = errno;
			free(pval);
			goto err_abort;
		}

		if (tx_hash_put(tx->wrset_lookup, field_key, prev_idx) < 0) {
			err = errno;
			free(pval);
			goto err_abort;
		}
	}

	return;

err_abort:
	DEBUGLOG("failed to append to wrset");
	norec_tx_abort(err);
}

void norec_tx_free(PMEMoid poid)
{
	norec_try_irrevoc();
	return tx_free(poid);
}

int validate_rdset(struct tx_vec *rdset)
{
	int i;
	for (i = 0; i < rdset->length; i++) {
		struct tx_vec_entry *e = &rdset->arr[i];

		DEBUGPRINT("[%d] validating...", tx_get_tid());
		if (memcmp(e->pval, e->addr, e->size))
			return 0;
	}
	return 1;
}

void persist_redo_wrset(struct tx_vec *wrset)
{
	struct tx_vec_entry *entry;
	int i;
	for (i = 0; i < wrset->length; i++) {
		entry = &wrset->arr[i];

		DEBUGPRINT("[%d] writing...", tx_get_tid());
		pmemobj_tx_add_range_direct(entry->addr, entry->size);
		memcpy(entry->addr, entry->pval, entry->size);
	}
}

void norec_try_irrevoc(void)
{
	struct tx_meta *tx = get_tx_meta();
	if (tx->irrevoc)
		return;
	
	while (!CAS(&glb, &tx->loc, tx->loc + 1)) {
		norec_validate();
	}
	persist_redo_wrset(tx->write_set);
	tx->loc++;
	tx->irrevoc = 1;
}

void norec_validate(void)
{
	struct tx_meta *tx = get_tx_meta();
	if (tx->irrevoc)
		return;

	int time;
	while (1) {
		time = glb;
		if (IS_ODD(time)) continue;

		CFENCE;
		if (!validate_rdset(tx->read_set))
			return norec_tx_restart();

		CFENCE;
		if (time == glb) {
			tx->loc = time;
			return;
		}
	}
}

int norec_prevalidate(void) {
	return get_tx_meta()->loc != glb;
}

void norec_thread_enter(PMEMobjpool *pop)
{
	struct tx_meta *tx = get_tx_meta();
	tx_thread_enter(pop);

	if (tx_vector_init(&tx->write_set))
		goto err_abort;

	if (tx_vector_init(&tx->read_set))
		goto err_abort;

	if (tx_hash_new(&tx->wrset_lookup))
		goto err_abort;

	return;

err_abort:
	DEBUGLOG("failed to init");
	abort();
}

void norec_thread_exit(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx_thread_exit();
	tx_vector_destroy(&tx->write_set);
	tx_vector_destroy(&tx->read_set);
	tx_hash_destroy(&tx->wrset_lookup);
}

int norec_tx_begin(jmp_buf env)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();
	struct tx_meta *tx = get_tx_meta();
	if (stage == TX_STAGE_NONE) {
		tx->irrevoc = 0;
		// do {
		// 	tx->loc = glb;
		// } while (IS_ODD(tx->loc));	
		tx->loc = glb & ~(1L);
	}
	
	return tx_begin(env);
}

void norec_tx_commit(void)
{
	struct tx_meta *tx = get_tx_meta();

	if (tx_get_level() > 0) {
		return pmemobj_tx_commit();
	}

	if (!tx->write_set->length) {
		return pmemobj_tx_commit();
	}

	if (tx->irrevoc) {
		tx_reclaim_frees();
		return pmemobj_tx_commit();
	}

	while (!CAS(&glb, &tx->loc, tx->loc + 1)) {
		norec_validate();
	}
	persist_redo_wrset(tx->write_set);
	CFENCE;
	glb = tx->loc + 2;

	DEBUGPRINT("[%d] committing...", tx_get_tid());
	tx_reclaim_frees();
	pmemobj_tx_commit();

}

void norec_tx_process(void)
{
	return tx_process(norec_tx_commit);
}

void norec_on_end(void)
{
	struct tx_meta *tx= get_tx_meta();

	tx_vector_empty(tx->write_set);
	tx_vector_empty(tx->read_set);
	tx_hash_empty(tx->wrset_lookup);
	
	if (tx_get_retry())
		return;
	
	/* release the lock on end */
	if (tx->irrevoc && IS_ODD(tx->loc)) {
		CFENCE;
		glb = tx->loc + 1;
	}
}

int norec_tx_end(void)
{
	return tx_end(norec_on_end);
}
