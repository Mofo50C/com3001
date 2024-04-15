#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>
#include <string.h>
#include "tml_base.h"
#include "tml_util.h"
#include "util.h"

struct tx_meta {
	enum tx_stage stage;
	struct tx_stack *entries;
	struct tx_vec *free_list;
	pid_t tid;
	int loc;
	int level;
	int retry;
	int last_errnum;
};

static struct tx_meta *get_tx_meta(void)
{
	static __thread struct tx_meta tx;
	return &tx;
}

enum tx_stage tx_get_stage(void)
{
	return get_tx_meta()->stage;
}

int tx_get_retry(void)
{
	return get_tx_meta()->retry;
}

pid_t tx_get_tid(void)
{
	return get_tx_meta()->tid;
}

int tx_get_error(void)
{
	return get_tx_meta()->last_errnum;
}

void _tx_abort(int errnum)
{	
	struct tx_meta *tx = get_tx_meta();

	if (errnum == 0)
		errnum = ECANCELED;

	if (errnum == -1 && tx->retry)
		tx->stage = TX_STAGE_ONRETRY;
	else
		tx->stage = TX_STAGE_ONABORT;
	
	struct tx_data *txd = tx->entries->head;

	tx->last_errnum = errnum;
	errno = errnum;

	if (!util_is_zeroed(txd->env, sizeof(jmp_buf)))
		longjmp(txd->env, errnum);
}

/* algorithm specific */

/* global lock */
static volatile _Atomic int glb = 0;

void tml_tx_abort(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx->retry = 1;
	_tx_abort(-1);
}

void tml_thread_enter(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx->tid = gettid();
	if (tx_vector_init(&tx->free_list)) {
		DEBUGLOG("failed to init free list");
		abort();
	}
}

void tml_thread_exit(void) {
	struct tx_meta *tx = get_tx_meta();
	tx_vector_empty(tx->free_list);
	tx_vector_destroy(&tx->free_list);
}

int tml_tx_begin(jmp_buf env)
{
	enum tx_stage stage = tx_get_stage();
	struct tx_meta *tx = get_tx_meta();
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

	do {
		tx->loc = glb;
	} while (IS_ODD(tx->loc));

	return 0;

err_abort:
	if (tx->stage == TX_STAGE_WORK)
		_tx_abort(err);
	else
		tx->stage = TX_STAGE_ONABORT;

	return err;
}

void tml_tx_write(void)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);
	
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
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);

	if (IS_EVEN(tx->loc) && glb != tx->loc)
		tml_tx_abort();
}

int tml_tx_free(void *ptr)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);

	int i;
	for (i = 0; i < tx->free_list->length; i++)
	{
		void *e = tx->free_list->addrs[i];
		if (e == ptr)
			return 0;
	}

	if (tx_vector_append(tx->free_list, ptr))
		_tx_abort(errno);

	return 0;
}

void *tml_tx_malloc(size_t size, int zero)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);
	
	int err = 0;
	void *tmp = malloc(size);
	if (tmp == NULL) {
		err = errno;
		goto err_abort;
	}

	if (zero)
		memset(tmp, 0, size);
	
	return tmp;

err_abort:
	_tx_abort(err);
	return NULL;
}


void tml_tx_commit(void)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);

	if (tx->level > 0) {
		tx->stage = TX_STAGE_ONCOMMIT;
		return;
	}

	int i;
	for (i = 0; i < tx->free_list->length; i++)
	{
		void *e = tx->free_list->addrs[i];
		tml_tx_write();
		free(e);
	}

	tx->stage = TX_STAGE_ONCOMMIT;

	if (IS_ODD(tx->loc)) {
		glb = tx->loc + 1;
	}
}

void tml_tx_process(void)
{
	struct tx_meta *tx = get_tx_meta();

	switch (tx->stage) {
	case TX_STAGE_NONE:
		break;
	case TX_STAGE_WORK:
		tml_tx_commit();
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

int tml_tx_end(void)
{	
	struct tx_meta *tx = get_tx_meta();

	struct tx_data *txd = tx_stack_pop(tx->entries);
	free(txd);

	int ret = tx->last_errnum;
	if (tx_stack_isempty(tx->entries)) {
		tx->stage = TX_STAGE_NONE;
		tx_vector_empty(tx->free_list);
	} else {
		tx->stage = TX_STAGE_WORK;
		tx->level--;
		if (tx->last_errnum)
			_tx_abort(tx->last_errnum);
	}

	if (tx->level > 0) {
		DEBUGLOG("failed to waterfall abort to outer tx");
		abort();
	}

	return ret;
}

