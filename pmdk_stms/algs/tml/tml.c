#define _GNU_SOURCE
#include <unistd.h>

#include <stdbool.h>
#include <stdatomic.h>
#include "tml_base.h"
#include "util.h"

struct tx_meta {
	PMEMobjpool *pop;
	pid_t tid;
	int loc;
	int retry;
	int level;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

/* global lock */
static volatile _Atomic int glb = 0;

int tml_get_retry(void)
{
	return get_tx_meta()->retry;
}

pid_t tml_get_tid(void)
{
	return get_tx_meta()->tid;
}

void tml_tx_abort(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx->retry = 1;
	pmemobj_tx_abort(-1);
}

void tml_preabort(void)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();
	struct tx_meta *tx = get_tx_meta();

	if (stage == TX_STAGE_ONABORT) {
		tx->level = 0;
	}
}

void tml_thread_enter(PMEMobjpool *pop)
{
	struct tx_meta *tx = get_tx_meta();
	tx->tid = gettid();
	tx->pop = pop;
}

void tml_thread_exit(void) {}

int tml_tx_begin(jmp_buf env)
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

void tml_tx_commit(void)
{
	pmemobj_tx_commit();
	// struct tx_meta *tx = get_tx_meta();
	// if (tx->level > 0) return;

	// if (IS_ODD(tx->loc)) {
	// 	glb = tx->loc + 1;
	// }
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
	} else if (!tx->retry) {
		/* release the lock on end (commit or abort) except when retrying */
		if (IS_ODD(tx->loc))
			glb = tx->loc + 1;
	}
	return pmemobj_tx_end();
}

void tml_tx_write(void)
{
	struct tx_meta *tx = get_tx_meta();
	if (IS_EVEN(tx->loc)) {
		if (!atomic_compare_exchange_strong(&glb, &tx->loc, tx->loc + 1)) {
			return tml_tx_abort();
		} else {
			tx->loc++;
		}
	}
}

void tml_tx_read(void)
{	
	struct tx_meta *tx = get_tx_meta();
	if (IS_EVEN(tx->loc) && glb != tx->loc)
		return tml_tx_abort();
}

