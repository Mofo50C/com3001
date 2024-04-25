#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>
#include <string.h>
#include "tx.h"
#include "tx_util.h"
#include "tml_base.h"
#include "util.h"

struct tx_meta {
	int loc;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

/* algorithm specific */

/* global lock */
static volatile _Atomic int glb = 0;

void tml_tx_abort(void)
{
	tx_restart();
}

void tml_thread_enter(void)
{
	tx_thread_enter();
}

void tml_thread_exit(void) {
	tx_thread_exit();
}

int tml_tx_begin(jmp_buf env)
{	
	enum tx_stage stage = tx_get_stage();
	struct tx_meta *tx = get_tx_meta();
	
	// DEBUGPRINT("[%d] begining", gettid());

	if (stage == TX_STAGE_NONE || stage == TX_STAGE_WORK) {
		do {
			tx->loc = glb;
		} while (IS_ODD(tx->loc));
	}

	return tx_begin(env);
}

void tml_tx_write(void)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_WORK(tx_get_stage());
	
	if (IS_EVEN(tx->loc)) {
		if (!atomic_compare_exchange_strong(&glb, &tx->loc, tx->loc + 1)) {
			tml_tx_abort();
		} else {
			tx->loc++;
		}
	}
}

void tml_tx_read(void)
{	
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_WORK(tx_get_stage());

	if (IS_EVEN(tx->loc) && glb != tx->loc)
		tml_tx_abort();
}

int tml_tx_free(void *ptr)
{
	return tx_free(ptr);
}

void *tml_tx_malloc(size_t size, int zero)
{
	return tx_malloc(size, zero);
}


void tml_tx_commit(void)
{
	tx_reclaim_frees();
	tx_commit();

	// if (IS_ODD(tx->loc)) {
	// 	glb = tx->loc + 1;
	// }
}

void tml_tx_process(void)
{
	tx_process(tml_tx_commit);
}

void tml_on_end(void)
{
	struct tx_meta *tx = get_tx_meta();
	if (!tx_get_retry()) {
		if (IS_ODD(tx->loc))
			glb = tx->loc + 1;
	}
}

int tml_tx_end(void)
{
	return tx_end(tml_on_end);
}

