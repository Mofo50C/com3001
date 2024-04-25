#ifndef DTML_H
#define DTML_H 1

#include <errno.h>
#include <util.h>
#include "dtml_base.h"

#define DTML_BEGIN()\
{\
	__label__ _TX_RETRY_LABEL;\
	jmp_buf _tx_env;\
	enum tx_stage _stage;\
	int _tx_errno;\
_TX_RETRY_LABEL:\
	if (setjmp(_tx_env)) {\
		errno = tx_get_error();\
	} else if ((_tx_errno = dtml_tx_begin(_tx_env))) {\
		errno = _tx_errno;\
	}\
	while ((_stage = tx_get_stage()) != DTML_STAGE_NONE) {\
		switch (_stage) {\
			case DTML_STAGE_WORK:

#define DTML_ONABORT\
				dtml_tx_process();\
				break;\
			case DTML_STAGE_ONABORT:

#define DTML_ONRETRY\
				dtml_tx_process();\
				break;\
			case DTML_STAGE_ONRETRY:

#define DTML_ONCOMMIT\
				dtml_tx_process();\
				break;\
			case DTML_STAGE_ONCOMMIT:

#define DTML_FINALLY\
				dtml_tx_process();\
				break;\
			case DTML_STAGE_FINALLY:

#define DTML_END\
				dtml_tx_process();\
				break;\
			default:\
				dtml_tx_process();\
				break;\
		}\
	}\
	if ((_tx_errno = dtml_tx_end())) {\
		if (_tx_errno == -1 && tx_get_retry()) {\
			DEBUGPRINT("[%d] retrying...", tx_get_tid());\
			goto _TX_RETRY_LABEL;\
		} else {\
			errno = _tx_errno;\
		}\
	}\
}

#define DTML_STARTUP()

#define DTML_SHUTDOWN()

#define DTML_ENTER dtml_thread_enter
#define DTML_EXIT dtml_thread_exit

#define DTML_WRITE_CHECK dtml_tx_write()
#define DTML_READ_CHECK dtml_tx_read()

#define DTML_WRITE(var, val)\
	DTML_WRITE_DIRECT(&(var), val, sizeof(val))

#define DTML_WRITE_DIRECT(p, value, sz) ({\
	DTML_WRITE_CHECK;\
	*(p) = (value);\
})

#define DTML_READ(var)\
	_DTML_READ(&(var), __typeof__(var))

#define DTML_READ_DIRECT(p, sz)\
	_DTML_READ(p, __typeof__(*(p)))

#define _DTML_READ(p, type) ({\
	type _ret = *(p);\
	DTML_READ_CHECK;\
	_ret;\
})

#define DTML_FREE(p)\
dtml_tx_free(p)

#define DTML_ALLOC(sz)\
dtml_tx_malloc(sz, 0)

#define DTML_ZALLOC(sz)\
dtml_tx_malloc(sz, 1)

#define DTML_NEW(type)\
dtml_tx_malloc(sizeof(type), 0)

#define DTML_ZNEW(type)\
dtml_tx_malloc(sizeof(type), 1)

#endif