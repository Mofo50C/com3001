#ifndef TX_GENERIC_H
#define TX_GENERIC_H 1

#include <libpmemobj.h>
#include "util.h"

#define _TX_BEGIN()\
{\
	__label__ _TX_RETRY_LABEL, _TX_EARLY_RETURN_LABEL;\
	jmp_buf _tx_env;\
	enum pobj_tx_stage _stage;\
	int _pobj_errno;\
_TX_RETRY_LABEL:\
	if (setjmp(_tx_env)) {\
		errno = pmemobj_tx_errno();\
	} else {\
		_pobj_errno = _TX_BEGIN_FUNC(_tx_env);\
		if (_pobj_errno)\
			errno = _pobj_errno;\
	}\
	while ((_stage = pmemobj_tx_stage()) != TX_STAGE_NONE) {\
		switch (_stage) {\
			case TX_STAGE_WORK:

#define _TX_ONABORT\
				_TX_PROCESS_FUNC();\
				break;\
			case TX_STAGE_ONABORT:\
				if (pmemobj_tx_errno() == -1 && _TX_GET_RETRY()) {/* do nothing if retrying...*/}\
				else

#define _TX_ONCOMMIT\
				_TX_PROCESS_FUNC();\
				break;\
			case TX_STAGE_ONCOMMIT:

#define _TX_FINALLY\
				_TX_PROCESS_FUNC();\
				break;\
			case TX_STAGE_FINALLY:

#define _TX_END\
				_TX_PROCESS_FUNC();\
				break;\
			default:\
_TX_EARLY_RETURN_LABEL:\
				TX_ONABORT_CHECK;\
				_TX_PROCESS_FUNC();\
				break;\
		}\
	}\
	_pobj_errno = _TX_END_FUNC();\
	if (_pobj_errno) {\
		if ((errno = _pobj_errno) == -1 && _TX_GET_RETRY()) {\
			DEBUGPRINT("[%d] retrying...", _TX_GET_TID());\
			goto _TX_RETRY_LABEL;\
		}\
	}\
}

#define _TX_RETURN()\
	goto _TX_EARLY_RETURN_LABEL

#endif