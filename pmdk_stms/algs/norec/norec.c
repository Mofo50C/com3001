#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include "tx.h"
#include "norec_base.h"
#include "norec_util.h"
#include "util.h"

#define VEC_INIT 8

struct tx_meta {
	pid_t tid;
	PMEMobjpool *pop;
	int loc;
	int retry;
	int level;
	int num_retries;
	int num_commits;
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
static _Atomic int glb = 0;

int norec_get_retry(void)
{
	return get_tx_meta()->retry;
}

pid_t norec_get_tid(void)
{
	return get_tx_meta()->tid;
}

void norec_tx_restart(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx->retry = 1;
	tx->num_retries++;
	pmemobj_tx_abort(-1);
}

void norec_tx_abort(void)
{
	pmemobj_tx_abort(0);
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
	pmemobj_tx_abort(err);
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
	pmemobj_tx_abort(err);
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

			DEBUGPRINT("[%d] validating...", tx->tid);
			if (memcmp(e->pval, e->addr, e->size))
				return norec_tx_restart();
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

void norec_thread_enter(PMEMobjpool *pop)
{
	struct tx_meta *tx = get_tx_meta();
	tx->tid = gettid();
	tx->pop = pop;
	tx->num_retries = 0;

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
	tx_add_metrics(tx->num_retries, tx->num_commits);
	tx_vector_destroy(&tx->write_set);
	tx_vector_destroy(&tx->read_set);
	tx_hash_destroy(&tx->wrset_lookup);
}

void norec_startup(void) {}

void norec_shutdown(void)
{
	tx_print_metrics();
}

int norec_tx_begin(jmp_buf env)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();
	struct tx_meta *tx = get_tx_meta();

	if (stage == TX_STAGE_NONE) {
		tx->retry = 0;

		do {
			tx->loc = glb;
		} while (IS_ODD(tx->loc));
	} else if (stage == TX_STAGE_WORK) {
		tx->level++;
	}
	
	return pmemobj_tx_begin(tx->pop, env, TX_PARAM_NONE, TX_PARAM_NONE);
}

void norec_tx_commit(void)
{
	struct tx_meta *tx = get_tx_meta();

	if ((tx->level > 0) || (tx->write_set->length == 0)) {
		return pmemobj_tx_commit();
	}

	while (!atomic_compare_exchange_strong(&glb, &tx->loc, tx->loc + 1)) {
		norec_validate();
	}

	struct tx_vec_entry *entry;
	int i;
	for (i = 0; i < tx->write_set->length; i++) {
		entry = &tx->write_set->arr[i];

		DEBUGPRINT("[%d] writing...", tx->tid);
		pmemobj_tx_add_range_direct(entry->addr, entry->size);
		memcpy(entry->addr, entry->pval, entry->size);

	}

	DEBUGPRINT("[%d] committing...", tx->tid);
	pmemobj_tx_commit();
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
		tx_vector_empty(tx->write_set);
		tx_vector_empty(tx->read_set);
		tx_hash_empty(tx->wrset_lookup);
		
		if (!tx->retry && !pmemobj_tx_errno()) {
			tx->num_commits++;
		}
		
		/* release the lock on end (commit or abort) except when retrying */
		if (!tx->retry)
			glb = tx->loc + 2;
	}

	return pmemobj_tx_end();
}