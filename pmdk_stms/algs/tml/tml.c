#define _GNU_SOURCE
#include <unistd.h>

#include <stdbool.h>
#include "tx.h"
#include "tx_util.h"
#include "tml_base.h"
#include "util.h"

struct tx_meta {
	uint64_t loc;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

/* global lock */
static uint64_t glb = 0;

void tml_tx_restart(void)
{
	return tx_restart();
}

void tml_tx_abort(int err)
{
	return pmemobj_tx_abort(err);
}

void tml_thread_enter(PMEMobjpool *pop)
{
	return tx_thread_enter(pop);
}

void tml_thread_exit(void)
{
	return tx_thread_exit();
}

int tml_tx_begin(jmp_buf env)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();
	struct tx_meta *tx = get_tx_meta();

	if (stage == TX_STAGE_NONE) {
		do {
			tx->loc = glb;
		} while (IS_ODD(tx->loc));
	}

	return tx_begin(env);
}

void tml_tx_commit(void)
{
	if (tx_get_level() > 0) {
		return pmemobj_tx_commit();
	}

	tx_reclaim_frees();
	pmemobj_tx_commit();
}

void tml_tx_process(void)
{
	return tx_process(tml_tx_commit);
}

void tml_on_end(void)
{	
	struct tx_meta *tx = get_tx_meta();

	if (tx_get_retry())
		return;

	/* release the lock on end */
	if (IS_ODD(tx->loc)) {
		CFENCE;
		glb = tx->loc + 1;
	}
}

int tml_tx_end(void)
{
	return tx_end(tml_on_end);
}

void tml_tx_free(PMEMoid poid)
{
	tml_try_irrevoc();
	return tx_free(poid);
}

void tml_try_irrevoc(void)
{
	struct tx_meta *tx = get_tx_meta();
	if (IS_EVEN(tx->loc)) {
		if (!CAS(&glb, &tx->loc, tx->loc + 1)) {
			return tml_tx_restart();
		} else {
			tx->loc++;
		}
	}
}

void tml_tx_write(void)
{
	return tml_try_irrevoc();
}

void tml_tx_read(void)
{	
	struct tx_meta *tx = get_tx_meta();

	CFENCE;
	if (IS_EVEN(tx->loc) && glb != tx->loc)
		return tml_tx_restart();
}

