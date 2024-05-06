#ifndef TX_GENERIC_H
#define TX_GENERIC_H 1

#define _TX_BEGIN()\
{\
	__label__ _TX_RETRY_LABEL, _TX_EARLY_RETURN_LABEL;\
	jmp_buf _tx_env;\
	enum tx_stage _stage;\
	int _tx_errno;\
_TX_RETRY_LABEL:\
	if (setjmp(_tx_env)) {\
		errno = tx_get_error();\
	} else if ((_tx_errno = _TX_BEGIN_FUNC(_tx_env))) {\
		errno = _tx_errno;\
	}\
	while ((_stage = tx_get_stage()) != TX_STAGE_NONE) {\
		switch (_stage) {\
			case TX_STAGE_WORK:

#define _TX_ONABORT\
				_TX_PROCESS_FUNC();\
				break;\
			case TX_STAGE_ONABORT:

#define _TX_ONRETRY\
				_TX_PROCESS_FUNC();\
				break;\
			case TX_STAGE_ONRETRY:

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
				_TX_PROCESS_FUNC();\
				break;\
		}\
	}\
	if ((_tx_errno = _TX_END_FUNC())) {\
		if (_tx_errno == -1 && tx_get_retry()) {\
			DEBUGPRINT("[%d] retrying...", tx_get_tid());\
			goto _TX_RETRY_LABEL;\
		} else {\
			errno = _tx_errno;\
		}\
	}\
}

#define _TX_RETURN()\
	goto _TX_EARLY_RETURN_LABEL

#endif