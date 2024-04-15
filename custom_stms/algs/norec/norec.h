#ifndef NOREC_H
#define NOREC_H 1

#include "norec_base.h"

#define NOREC_BEGIN()\
{\
	__label__ _TX_RETRY_LABEL;\
	jmp_buf _tx_env;\
	enum tx_stage _stage;\
	int _tx_errno;\
_TX_RETRY_LABEL:\
	if (setjmp(_tx_env)) {\
		errno = tx_get_error();\
	} else {\
		if (_tx_errno = norec_tx_begin(_tx_env))\
			errno = _tx_errno;\
	}\
	while ((_stage = tx_get_stage()) != TX_STAGE_NONE) {\
		switch (_stage) {\
			case TX_STAGE_WORK:

#define NOREC_ONABORT\
				norec_tx_process();\
				break;\
			case TX_STAGE_ONABORT:

#define NOREC_ONRETRY\
				norec_tx_process();\
				break;\
			case TX_STAGE_ONRETRY:

#define NOREC_ONCOMMIT\
				norec_tx_process();\
				break;\
			case TX_STAGE_ONCOMMIT:

#define NOREC_FINALLY\
				norec_tx_process();\
				break;\
			case TX_STAGE_FINALLY:

#define NOREC_END\
				norec_tx_process();\
				break;\
			default:\
				norec_tx_process();\
				break;\
		}\
	}\
	if ((_tx_errno = norec_tx_end())) {\
		if (_tx_errno == -1 && tx_get_retry()) {\
			DEBUGPRINT("[%d] retrying...", tx_get_tid());\
			goto _TX_RETRY_LABEL;\
		} else {\
			errno = _tx_errno;\
		}\
	}\
}

#define NOREC_ENTER norec_thread_enter
#define NOREC_EXIT norec_thread_exit


/* valp is pointer to variable */
#define NOREC_WRITE_P(var, valp)\
	NOREC_WRITE_P_DIRECT(&(var), valp, sizeof(__typeof__(*(valp))))

#define NOREC_WRITE_P_DIRECT(pdirect_field, valp, size) ({\
	void *_pval = malloc(size);\
	memcpy(_pval, valp, size);\
	norec_tx_write(pdirect_field, size, _pval);\
})

/* val is literal value to be written */
#define NOREC_WRITE(var, val)\
	_NOREC_WRITE(&(var), val, __typeof__(val), sizeof(__typeof__(val)))

#define NOREC_WRITE_DIRECT(pdirect_field, val, sz)\
	_NOREC_WRITE(pdirect_field, val, __typeof__(val), sz)

#define _NOREC_WRITE(pdirect_field, val, t, sz) ({\
	t _val = val;\
	t *_pval = malloc(sz);\
	memcpy(_pval, &_val, sz);\
	norec_tx_write(pdirect_field, sz, _pval);\
})

/* pval is is pointer to dynamic memory in heap
 * doesn't need malloc'ing again
 */
#define NOREC_WRITE_PDYN(var, pval)\
	NOREC_WRITE_PDYN_DIRECT(&(var), pval, sizeof(*(pval)))

#define NOREC_WRITE_PDYN_DIRECT(pdirect_field, pval, sz)\
norec_tx_write(pdirect_field, sz, pval)

/* shared reads */
#define NOREC_READ(var)\
	_NOREC_READ(&(var), __typeof__(var), sizeof(__typeof__(var)))

#define NOREC_READ_DIRECT(p, sz)\
	_NOREC_READ(p, __typeof__(*(p)), sz)

#define _NOREC_READ(p, t, sz) ({\
	t _ret;\
	if (!norec_wrset_get(p, &_ret, sz)) {\
		_ret = *(p);\
		while (norec_prevalidate()) {\
			norec_validate();\
			_ret = *(p);\
		}\
		norec_rdset_add(p, &_ret, sz);\
	}\
	_ret;\
})

#define NOREC_FREE(p)\
norec_tx_free(p)

#define NOREC_ALLOC(sz)\
norec_tx_malloc(sz, 0)

#define NOREC_ZALLOC(sz)\
norec_tx_malloc(sz, 1)

#define NOREC_NEW(type)\
norec_tx_malloc(sizeof(type), 0)

#define NOREC_ZNEW(type)\
norec_tx_malloc(sizeof(type), 1)

#endif