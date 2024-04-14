#include <stdbool.h>
#include <stdatomic.h>
#include "txpmdk_base.h"
#include "util.h"

struct tx_meta {
	PMEMobjpool *pop;
	pid_t tid;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

pid_t txpmdk_get_tid(void)
{
	return get_tx_meta()->tid;
}

void txpmdk_startup(PMEMobjpool *pop)
{
	struct tx_meta *tx = get_tx_meta();
	tx->tid = gettid();
	tx->pop = pop;
}

void txpmdk_cleanup(void) {}

void txpmdk_preabort(void) {}

int txpmdk_begin(jmp_buf env)
{
	struct tx_meta *tx = get_tx_meta();
	return pmemobj_tx_begin(tx->pop, env, TX_PARAM_NONE, TX_PARAM_NONE);
}

