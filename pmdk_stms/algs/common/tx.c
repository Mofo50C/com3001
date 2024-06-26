#define _GNU_SOURCE
#include <unistd.h>

#include <stdatomic.h>
#include <stdio.h>

#include <libpmemobj.h>
#include "util.h"
#include "tx.h"
#include "tx_util.h"

struct tx {
	pid_t tid;
	PMEMobjpool *pop;
	int retry;
	int level;
	int num_retries;
	int num_commits;
};

static _Atomic uint64_t total_retries = 0;
static _Atomic uint64_t total_commits = 0;

static struct tx *get_tx(void)
{
	static __thread struct tx tx;
	return &tx;
}

int tx_get_retry(void)
{
	return get_tx()->retry;
}

pid_t tx_get_tid(void)
{
	return get_tx()->tid;
}

int tx_get_level(void)
{
	return get_tx()->level;
}

int tx_begin(jmp_buf env)
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();
	struct tx *tx = get_tx();

	if (stage == TX_STAGE_NONE) {
		tx->retry = 0;
	} else if (stage == TX_STAGE_WORK) {
		tx->level++;
	}

	return pmemobj_tx_begin(tx->pop, env, TX_PARAM_NONE, TX_PARAM_NONE);
}

void tx_process(void (*commit_cb)(void))
{
	enum pobj_tx_stage stage = pmemobj_tx_stage();

	if (stage == TX_STAGE_WORK) {
		commit_cb();
	} else {
		pmemobj_tx_process();
	}
}

void tx_restart(void)
{
	struct tx *tx = get_tx();
	tx->retry = 1;
	tx->num_retries++;
	pmemobj_tx_abort(-1);
}

void tx_thread_enter(PMEMobjpool *pop)
{
	struct tx *tx = get_tx();
	tx->tid = gettid();
	tx->pop = pop;
	tx->num_retries = 0;

	return;
}

void tx_thread_exit(void)
{
	struct tx *tx = get_tx();
	tx_add_metrics(tx->num_retries, tx->num_commits);
}

void tx_startup(void) {}

void tx_shutdown(void)
{
	tx_print_metrics();
}

int tx_end(void (*end_cb)(void))
{
	struct tx *tx = get_tx();
	if (tx->level > 0) {
		tx->level--;
	} else {
		if (!tx->retry && !pmemobj_tx_errno()) {
			tx->num_commits++;
		}
		end_cb();
	}

	return pmemobj_tx_end();
}

void tx_add_metrics(int retries, int commits)
{
	atomic_fetch_add_explicit(&total_retries, retries, memory_order_relaxed);
	atomic_fetch_add_explicit(&total_commits, commits, memory_order_relaxed);
}

void tx_print_metrics(void)
{
	printf("[[TM METRICS]]\n");
    printf("\tTotal retries: %ju\n", total_retries);
    printf("\tTotal commits: %ju\n", total_commits);
}