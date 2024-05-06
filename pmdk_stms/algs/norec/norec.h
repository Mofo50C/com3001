#ifndef NOREC_H
#define NOREC_H 1

#include "norec_base.h"

/* define stm-specific functions */
#define _TX_BEGIN_FUNC norec_tx_begin
#define _TX_PROCESS_FUNC norec_tx_process
#define _TX_END_FUNC norec_tx_end
#define _TX_GET_RETRY norec_get_retry
#define _TX_GET_TID norec_get_tid
/* end define generics */

#include "tx_generic.h"

#define NOREC_ENTER(pop)\
norec_thread_enter(pop)

#define NOREC_EXIT()\
norec_thread_exit()

/* define aliases for macros */
#define NOREC_BEGIN _TX_BEGIN
#define NOREC_END _TX_END
#define NOREC_ONABORT _TX_ONABORT
#define NOREC_ONCOMMIT _TX_ONCOMMIT
#define NOREC_FINALLY _TX_FINALLY
#define NOREC_RETURN _TX_RETURN
/* end define aliases */

/* val is literal value to be written */
#define NOREC_WRITE_FIELD(o, field, val)\
	NOREC_WRITE(D_RW(o)->field, val)

#define NOREC_WRITE(var, val)\
	_NOREC_WRITE(&(var), val, __typeof__(val), sizeof(val))

#define NOREC_WRITE_DIRECT(pdirect_field, val, sz)\
	_NOREC_WRITE(pdirect_field, val, __typeof__(val), sz)

#define _NOREC_WRITE(pdirect_field, val, t, sz) ({\
	t _buf = val;\
	norec_tx_write(pdirect_field, sz, &_buf);\
})

/* shared reads */
#define NOREC_READ_FIELD(o, field)\
	NOREC_READ(D_RW(o)->field)

#define NOREC_READ(var)\
	_NOREC_READ(&(var), __typeof__(var), sizeof(var))

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

// #define NOREC_FREE TX_FREE
#define NOREC_FREE(o)\
norec_tx_free((o).oid)

#define NOREC_ALLOC TX_ALLOC
#define NOREC_NEW TX_NEW
#define NOREC_ZNEW TX_ZNEW
#define NOREC_ZALLOC TX_ZALLOC

#define NOREC_ABORT norec_tx_abort
#define NOREC_RESTART norec_tx_restart

#endif