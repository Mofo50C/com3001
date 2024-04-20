#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "tx_util.h"
#include "tx.h"

struct tx {
	enum tx_stage stage;
	struct tx_stack *entries;
	struct tx_vec *free_list;
	struct tx_vec *alloc_list;
	pid_t tid;
	int level;
	int retry;
	int last_errnum;
};

static struct tx *get_tx(void)
{
	static __thread struct tx tx;
	return &tx;
}

enum tx_stage tx_get_stage(void)
{
	return get_tx()->stage;
}

int tx_get_retry(void)
{
	return get_tx()->retry;
}

void tx_restart(void)
{
	struct tx *tx = get_tx();
	tx->retry = 1;
	tx_abort(-1);
}

int tx_get_level(void)
{
	return get_tx()->level;
}

pid_t tx_get_tid(void)
{
	return get_tx()->tid;
}

int tx_get_error(void)
{
	return get_tx()->last_errnum;
}

void tx_abort(int errnum)
{	
	struct tx *tx = get_tx();

	if (errnum == 0)
		errnum = ECANCELED;

	if (errnum == -1 && tx->retry)
		tx->stage = TX_STAGE_ONRETRY;
	else
		tx->stage = TX_STAGE_ONABORT;
	
	struct tx_data *txd = tx->entries->head;
	if (txd->next == NULL && tx->level == 0) {
		int i;
		for (i = 0; i < tx->alloc_list->length; i++) {
			struct tx_vec_entry *e = &tx->alloc_list->arr[i];
			free(e->addr);
		}
	}

	tx->last_errnum = errnum;
	errno = errnum;

	if (!tx_util_is_zeroed(txd->env, sizeof(jmp_buf)))
		longjmp(txd->env, errnum);
}

void tx_thread_enter(void)
{
	struct tx *tx = get_tx();
	tx->tid = gettid();
	if (tx_vector_init(&tx->free_list))
		goto err_abort;

	if (tx_vector_init(&tx->alloc_list))
		goto err_abort;

	return;

err_abort:
	DEBUGLOG("failed to init");
	abort();
}

void tx_thread_exit(void) 
{
	struct tx *tx = get_tx();
	tx_vector_empty(tx->free_list);
	tx_vector_destroy(&tx->free_list);

	tx_vector_empty_unsafe(tx->alloc_list);
	tx_vector_destroy(&tx->alloc_list);
}

int tx_begin(jmp_buf env)
{
	enum tx_stage stage = tx_get_stage();
	struct tx *tx = get_tx();
	int err = 0;

	tx->retry = 0;
	if (stage == TX_STAGE_NONE) {
		if (tx_stack_init(&tx->entries)) {
			err = errno;
			goto err_abort;
		}

		tx->level = 0;
	} else if (stage == TX_STAGE_WORK) {
		tx->level++;
	} else {
		DEBUGLOG("called begin at wrong stage");
		abort();
	}

	struct tx_data *txd = malloc(sizeof(struct tx_data));
	if (txd == NULL) {
		err = errno;
		goto err_abort;
	}
	
	txd->next = NULL;
	if (env != NULL) {
		memcpy(txd->env, env, sizeof(jmp_buf));
	} else {
		memset(txd->env, 0, sizeof(jmp_buf));
	}
	
	tx_stack_push(tx->entries, txd);
	tx->stage = TX_STAGE_WORK;

	return 0;

err_abort:
	DEBUGLOG("tx failed to start");
	if (tx->stage == TX_STAGE_WORK)
		tx_abort(err);
	else
		tx->stage = TX_STAGE_ONABORT;

	return err;
}

void *tx_malloc(size_t size, int zero)
{
	struct tx *tx = get_tx();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);
	
	int err = 0;
	void *addr = malloc(size);
	if (addr == NULL) {
		err = errno;
		goto err_abort;
	}

	if (zero)
		memset(addr, 0, size);

	struct tx_vec_entry e = {
		.addr = addr
	};
	if (tx_vector_append(tx->alloc_list, &e) < 0) {
		err = errno;
		goto err_clean;
	}
	
	return addr;

err_clean:
	free(addr);
err_abort:
	DEBUGLOG("tx_malloc failed");
	tx_abort(err);
	return NULL;
}

int tx_free(void *ptr)
{
	struct tx *tx = get_tx();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);

	int i;
	for (i = 0; i < tx->free_list->length; i++)
	{
		struct tx_vec_entry *e = &tx->free_list->arr[i];
		if (e->addr == ptr)
			return 0;
	}

	struct tx_vec_entry e = {
		.addr = ptr,
		.pval = NULL,
		.size = 0
	};

	if (tx_vector_append(tx->free_list, &e) < 0) {
		DEBUGLOG("tx_free failed");
		tx_abort(errno);
        return 1;
	}

	return 0;
}

void tx_process(void (*commit_cb)(void))
{
	struct tx *tx = get_tx();

	switch (tx->stage) {
	case TX_STAGE_NONE:
		break;
	case TX_STAGE_WORK:
		commit_cb();
		break;
	case TX_STAGE_ONRETRY:
	case TX_STAGE_ONABORT:
	case TX_STAGE_ONCOMMIT:
		tx->stage = TX_STAGE_FINALLY;
		break;
	case TX_STAGE_FINALLY:
		tx->stage = TX_STAGE_NONE;
		break;
	default:
		DEBUGLOG("process invalid stage");
		abort();
	}
}

int tx_end(void (*end_cb)(void))
{	
	struct tx *tx = get_tx();

	struct tx_data *txd = tx_stack_pop(tx->entries);
	free(txd);

	int ret = tx->last_errnum;
	if (tx_stack_isempty(tx->entries)) {
		tx->stage = TX_STAGE_NONE;
		tx_vector_empty_unsafe(tx->alloc_list);
		tx_vector_empty(tx->free_list);

		end_cb();
	} else {
		tx->stage = TX_STAGE_WORK;
		tx->level--;
		if (tx->last_errnum)
			tx_abort(tx->last_errnum);
	}

	if (tx->level > 0) {
		DEBUGLOG("failed to waterfall abort to outer tx");
		abort();
	}

	return ret;
}

void tx_commit(void)
{
	struct tx *tx = get_tx();
	tx->stage = TX_STAGE_ONCOMMIT;
}

void tx_reclaim_frees(void)
{
	struct tx *tx = get_tx();
	struct tx_vec_entry *entry;
	int i;
	for (i = 0; i < tx->free_list->length; i++) {
		entry = &tx->free_list->arr[i];

		DEBUGPRINT("[%d] freeing...", tx->tid);
		free(entry->addr);
	}
}