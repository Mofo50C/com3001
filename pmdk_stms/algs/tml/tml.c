#define _GNU_SOURCE
#include <unistd.h>

#include <stdbool.h>
#include <stdatomic.h>
#include "tx.h"
#include "tx_util.h"
#include "tml_base.h"
#include "util.h"

struct tx_meta {
	PMEMobjpool *pop;
	pid_t tid;
	int loc;
	int retry;
	int level;
	int num_retries;
	int num_commits;
	struct tx_vec *free_list;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

/* global lock */
static _Atomic int glb = 0;

int tml_get_retry(void)
{
	return get_tx_meta()->retry;
}

pid_t tml_get_tid(void)
{
	return get_tx_meta()->tid;
}

void tml_tx_restart(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx->retry = 1;
	tx->num_retries++;
	pmemobj_tx_abort(-1);
}

void tml_tx_abort(void)
{
	pmemobj_tx_abort(0);
}

void tml_thread_enter(PMEMobjpool *pop)
{
	struct tx_meta *tx = get_tx_meta();
	tx->tid = gettid();
	tx->pop = pop;
	tx->num_retries = 0;
	if (tx_vector_init(&tx->free_list))
		goto err_abort;

	return;

err_abort:
	DEBUGLOG("failed to init");
	abort();
}

void tml_thread_exit(void) {
	struct tx_meta *tx = get_tx_meta();
	tx_add_metrics(tx->num_retries, tx->num_commits);
	tx_vector_destroy(&tx->free_list);
}

void tml_startup(void) {}

void tml_shutdown(void)
{
	tx_print_metrics();
}

int tml_tx_begin(jmp_buf env)
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

void tx_reclaim_frees(void)
{
	struct tx_meta *tx = get_tx_meta();
	struct tx_vec_entry *entry;
	int i;
	for (i = 0; i < tx->free_list->length; i++) {
		entry = &tx->free_list->arr[i];

		DEBUGPRINT("[%d] freeing...", tx->tid);
		if (!OID_IS_NULL(entry->oid))
			pmemobj_tx_free(entry->oid);
	}
}

void tml_tx_commit(void)
{
	struct tx_meta *tx = get_tx_meta();
	if (tx->level == 0) {
		tx_reclaim_frees();
	}
	pmemobj_tx_commit();
}

void tml_tx_process(void)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();

	if (stage == TX_STAGE_WORK) {
		tml_tx_commit();
	} else {
		pmemobj_tx_process();
	}
}

int tml_tx_end(void)
{	
	struct tx_meta *tx = get_tx_meta();
	if (tx->level > 0) {
		tx->level--;
	} else {
		tx_vector_clear(tx->free_list);

		if (!tx->retry && !pmemobj_tx_errno()) {
			tx->num_commits++;
		}

		/* release the lock on end (commit or abort) except when retrying */
		if (!tx->retry && IS_ODD(tx->loc))
			glb = tx->loc + 1;
	}
	return pmemobj_tx_end();
}

void tml_tx_free(PMEMoid poid)
{
	struct tx_meta *tx = get_tx_meta();

	int i;
	for (i = 0; i < tx->free_list->length; i++)
	{
		struct tx_vec_entry *e = &tx->free_list->arr[i];
		if (OID_EQUALS(e->oid, poid)) {
			e->oid = OID_NULL;
			break;
		}
	}

	struct tx_vec_entry e = {
		.oid = poid,
		.addr = NULL,
		.pval = NULL,
		.size = 0
	};

	if (tx_vector_append(tx->free_list, &e, NULL)) {
		DEBUGLOG("tx_free failed");
		pmemobj_tx_abort(errno);
	}
}

void tml_tx_write(void)
{
	struct tx_meta *tx = get_tx_meta();
	if (IS_EVEN(tx->loc)) {
		if (!atomic_compare_exchange_strong(&glb, &tx->loc, tx->loc + 1)) {
			return tml_tx_restart();
		} else {
			tx->loc++;
		}
	}
}

void tml_tx_read(void)
{	
	struct tx_meta *tx = get_tx_meta();
	if (IS_EVEN(tx->loc) && glb != tx->loc)
		return tml_tx_restart();
}

