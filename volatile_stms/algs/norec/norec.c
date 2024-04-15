#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>
#include <string.h>
#include "norec_base.h"
#include "norec_util.h"
#include "util.h"

struct tx_meta {
	enum tx_stage stage;
	struct tx_stack *entries;
	struct tx_vec *free_list;
	pid_t tid;
	int level;
	int last_errnum;
	int retry;
	int loc;
	struct tx_vec *read_set;
	struct tx_vec *write_set;
	struct tx_hash *wrset_lookup;
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

void norec_tx_abort(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx->retry = 1;
	_tx_abort(-1);
}

void norec_thread_enter(void)
{
	struct tx_meta *tx = get_tx_meta();
	tx->tid = gettid();
	if (tx_vector_init(&tx->free_list))
		goto err_abort;

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

void norec_thread_exit(void) {
	struct tx_meta *tx = get_tx_meta();
	tx_vector_empty(tx->free_list);
	tx_vector_destroy(&tx->free_list);

	tx_vector_empty(tx->write_set);
	tx_vector_destroy(&tx->write_set);

	tx_vector_empty(tx->read_set);
	tx_vector_destroy(&tx->read_set);

	tx_hash_destroy(&tx->wrset_lookup);
}

int norec_tx_begin(jmp_buf env)
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

void norec_rdset_add(void *pdirect, void *src, size_t size)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);
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

	if (tx_vector_append(tx->read_set, &e)) {
		err = errno;
		goto err_abort;
	}

	return;

err_abort:
	free(pval);
	_tx_abort(err);
}

bool norec_wrset_get(void *pdirect, void *buf, size_t size)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);

	uintptr_t key = (uintptr_t)pdirect;

	struct tx_hash_entry *entry = tx_hash_get(tx->wrset_lookup, key);
	if (entry == NULL)
		return false;
	
	void *pval = tx->write_set->arr[entry->index].pval;
	memcpy(buf, pval, size);
	return true;
}

void norec_tx_write(void *pdirect_field, size_t size, void *pval)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);

	uintptr_t field_key = (uintptr_t)pdirect_field;

	struct tx_hash_entry *entry;
	if ((entry = tx_hash_get(tx->wrset_lookup, field_key))) {
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
		size_t idx = tx_vector_append(tx->write_set, &e);
		tx_hash_put(tx->wrset_lookup, field_key, idx);
	}
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
				return norec_tx_abort();
		}

		if (time == glb) {
			tx->loc = time;
			return;
		}
	}
}

bool norec_prevalidate(void) {
	struct tx_meta *tx = get_tx_meta();
	return tx->loc != glb;
}

int norec_tx_free(void *ptr)
{
	struct tx_meta *tx = get_tx_meta();
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

	if (tx_vector_append(tx->free_list, &e))
		_tx_abort(errno);

	return 0;
}

void *norec_tx_malloc(size_t size, int zero)
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


void norec_tx_commit(void)
{
	struct tx_meta *tx = get_tx_meta();
	ASSERT_IN_STAGE(tx, TX_STAGE_WORK);

	if ((tx->level > 0) || (tx->write_set->length == 0)) {
		tx->stage = TX_STAGE_ONCOMMIT;
		return;
	}

	while (!atomic_compare_exchange_strong(&glb, &tx->loc, tx->loc + 1)) {
		norec_validate();
	}

	struct tx_vec_entry *entry;
	int i;

	// since this doesn't use pmemobj snapshotting, need to free BEFORE we write, 
	// since the writes are instantly visible to (volatile) memory.
	for (i = 0; i < tx->write_set->length; i++) {
		entry = &tx->free_list->arr[i];

		DEBUGPRINT("[%d] freeing...", tx->tid);
		free(entry->addr);
	}

	for (i = 0; i < tx->write_set->length; i++) {
		entry = &tx->write_set->arr[i];

		DEBUGPRINT("[%d] writing...", tx->tid);
		memcpy(entry->addr, entry->pval, entry->size);
	}

	tx->stage = TX_STAGE_ONCOMMIT;

	glb = tx->loc + 2;
}

void norec_tx_process(void)
{
	struct tx_meta *tx = get_tx_meta();

	switch (tx->stage) {
	case TX_STAGE_NONE:
		break;
	case TX_STAGE_WORK:
		norec_tx_commit();
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

int norec_tx_end(void)
{	
	struct tx_meta *tx = get_tx_meta();

	struct tx_data *txd = tx_stack_pop(tx->entries);
	free(txd);

	int ret = tx->last_errnum;
	if (tx_stack_isempty(tx->entries)) {
		tx->stage = TX_STAGE_NONE;
		tx_vector_empty(tx->free_list);
		tx_vector_empty(tx->write_set);
		tx_vector_empty(tx->read_set);
		tx_hash_empty(tx->wrset_lookup);
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

