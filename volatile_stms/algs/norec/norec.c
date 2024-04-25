#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>
#include <string.h>
#include "tx.h"
#include "tx_util.h"
#include "norec_base.h"
#include "norec_util.h"
#include "util.h"

struct tx_meta {
	int loc;
	struct tx_vec *read_set;
	struct tx_vec *write_set;
	struct tx_hash *wrset_lookup;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

/* algorithm specific */

/* global lock */
static volatile _Atomic int glb = 0;

void norec_tx_abort(void)
{
	tx_restart();
}

void norec_thread_enter(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx_thread_enter();

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

void norec_thread_exit(void) {
	struct tx_meta *tx = get_tx_meta();
	tx_thread_exit();

	tx_vector_empty(tx->write_set);
	tx_vector_destroy(&tx->write_set);

	tx_vector_empty(tx->read_set);
	tx_vector_destroy(&tx->read_set);

	tx_hash_destroy(&tx->wrset_lookup);
}

int norec_tx_begin(jmp_buf env)
{
	enum tx_stage stage = tx_get_stage();
	struct tx_meta *tx = get_tx_meta();

	if (stage == TX_STAGE_NONE || stage == TX_STAGE_WORK) {
		do {
			tx->loc = glb;
		} while (IS_ODD(tx->loc));
	}

	return tx_begin(env);
}

void norec_rdset_add(void *pdirect, void *buf, size_t size)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_WORK(tx_get_stage())
	int err = 0;

	void *pval = malloc(size);
	if (pval == NULL) {
		err = errno;
		goto err_abort;
	}

	memcpy(pval, buf, size);

	struct tx_vec_entry e = {
		.addr = pdirect,
		.pval = pval,
		.size = size
	};

	if (tx_vector_append(tx->read_set, &e) < 0) {
		err = errno;
		goto err_clean;
	}

	return;

err_clean:
	free(pval);
err_abort:
	DEBUGLOG("tx_read failed");
	tx_abort(err);
}

bool norec_wrset_get(void *pdirect, void *retbuf, size_t size)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_WORK(tx_get_stage());

	uintptr_t key = (uintptr_t)pdirect;

	struct tx_hash_entry *entry = tx_hash_get(tx->wrset_lookup, key);
	if (entry == NULL)
		return false;
	
	void *pval = tx->write_set->arr[entry->index].pval;
	memcpy(retbuf, pval, size);
	return true;
}

void norec_tx_write(void *pdirect_field, size_t size, void *buf)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_WORK(tx_get_stage());

	int err = 0;
	void *pval = malloc(size);
	if (pval == NULL) {
		err = errno;
		goto err_abort;
	}

	memcpy(pval, buf, size);

	uintptr_t field_key = (uintptr_t)pdirect_field;

	struct tx_hash_entry *entry;
	if ((entry = tx_hash_get(tx->wrset_lookup, field_key))) {
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
		size_t idx = tx_vector_append(tx->write_set, &e);
		if (idx < 0) {
			err = errno;
			goto err_clean;
		}

		if (tx_hash_put(tx->wrset_lookup, field_key, idx) < 0) {
			err = errno;
			goto err_clean;
		}
	}

	return;

err_clean:
	free(pval);
err_abort:
	DEBUGLOG("tx_write failed");
	tx_abort(err);
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
			struct tx_vec_entry *e = &tx->read_set->arr[i];

			DEBUGPRINT("[%d] validating...", tx_get_tid());
			if (memcmp(e->pval, e->addr, e->size))
				return norec_tx_abort();
		}

		if (time == glb) {
			tx->loc = time;
			return;
		}
	}
}

bool norec_prevalidate(void) {
	struct tx_meta *tx = get_tx_meta();
	return tx->loc != glb;
}

int norec_tx_free(void *ptr)
{
	return tx_free(ptr);
}

void *norec_tx_malloc(size_t size, int zero)
{
	return tx_malloc(size, zero);
}

void norec_tx_commit(void)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_WORK(tx_get_stage());

	if ((tx_get_level() > 0) || (tx->write_set->length == 0)) {
		return tx_commit();
	}

	while (!atomic_compare_exchange_strong(&glb, &tx->loc, tx->loc + 1)) {
		norec_validate();
	}

	struct tx_vec_entry *entry;
	int i;

	for (i = 0; i < tx->write_set->length; i++) {
		entry = &tx->write_set->arr[i];

		DEBUGPRINT("[%d] writing...", tx_get_tid());
		memcpy(entry->addr, entry->pval, entry->size);
	}

	tx_reclaim_frees();
	tx_commit();

	// glb = tx->loc + 2;
}

void norec_tx_process(void)
{
	tx_process(norec_tx_commit);
}

void norec_on_end(void)
{	
	struct tx_meta *tx = get_tx_meta();
	tx_vector_empty(tx->write_set);
	tx_vector_empty(tx->read_set);
	tx_hash_empty(tx->wrset_lookup);

	/* release lock if we were writing tx */
	if (!tx_get_retry())
		glb = tx->loc + 2;
}

int norec_tx_end(void)
{	
	return tx_end(norec_on_end);
}

