#ifndef TML_H
#define TML_H 1

#include "tml_base.h"

#define TML_BEGIN()\
{\
	__label__ _TX_RETRY_LABEL;\
	jmp_buf _tx_env;\
	enum tx_stage _stage;\
	int _tx_errno;\
_TX_RETRY_LABEL:\
	if (setjmp(_tx_env)) {\
		errno = tx_get_error();\
	} else {\
		if (_tx_errno = tml_tx_begin(_tx_env))\
			errno = _tx_errno;\
	}\
	while ((_stage = tx_get_stage()) != TX_STAGE_NONE) {\
		switch (_stage) {\
			case TX_STAGE_WORK:

#define TML_ONABORT\
				tml_tx_process();\
				break;\
			case TX_STAGE_ONABORT:

#define TML_ONRETRY\
				tml_tx_process();\
				break;\
			case TX_STAGE_ONRETRY:

#define TML_ONCOMMIT\
				tml_tx_process();\
				break;\
			case TX_STAGE_ONCOMMIT:

#define TML_FINALLY\
				tml_tx_process();\
				break;\
			case TX_STAGE_FINALLY:

#define TML_END\
				tml_tx_process();\
				break;\
			default:\
				tml_tx_process();\
				break;\
		}\
	}\
	if ((_tx_errno = tml_tx_end())) {\
		if (_tx_errno == -1 && tx_get_retry()) {\
			DEBUGPRINT("[%d] retrying...", tx_get_tid());\
			goto _TX_RETRY_LABEL;\
		} else {\
			errno = _tx_errno;\
		}\
	}\
}

#define TML_ENTER tml_thread_enter
#define TML_EXIT tml_thread_exit

#define TML_WRITE_CHECK tml_tx_write()
#define TML_READ_CHECK tml_tx_read()

#define TML_WRITE(var, val)\
	TML_WRITE_DIRECT(&(var), val)

#define TML_WRITE_DIRECT(p, value, sz) ({\
	TML_WRITE_CHECK;\
	*(p) = (value);\
})

#define TML_READ(var)\
	_TML_READ(&(var), __typeof__(var))

#define TML_READ_DIRECT(p, sz)\
	_TML_READ(p, __typeof__(*(p)))

#define _TML_READ(p, type) ({\
	type _ret = *(p);\
	TML_READ_CHECK;\
	_ret;\
})

#define TML_FREE(p)\
tml_tx_free(p)

#define TML_ALLOC(sz)\
tml_tx_malloc(sz, 0)

#define TML_ZALLOC(sz)\
tml_tx_malloc(sz, 1)

#define TML_NEW(type)\
tml_tx_malloc(sizeof(type), 0)

#define TML_ZNEW(type)\
tml_tx_malloc(sizeof(type), 1)

#endif