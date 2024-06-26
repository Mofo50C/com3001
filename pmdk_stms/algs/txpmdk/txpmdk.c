#define _GNU_SOURCE
#include <unistd.h>

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

int txpmdk_get_retry(void)
{
	return 0;
}

void txpmdk_thread_enter(PMEMobjpool *pop)
{
	struct tx_meta *tx = get_tx_meta();
	tx->tid = gettid();
	tx->pop = pop;
}

void txpmdk_thread_exit(void) {}

int txpmdk_begin(jmp_buf env)
{
	struct tx_meta *tx = get_tx_meta();
	return pmemobj_tx_begin(tx->pop, env, TX_PARAM_NONE, TX_PARAM_NONE);
}

