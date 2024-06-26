#ifndef TML_H
#define TML_H 1

#include "tx.h"
#include "tml_base.h"

/* define the generic macros with current stm-specific functions */
#define _TX_BEGIN_FUNC tml_tx_begin
#define _TX_PROCESS_FUNC tml_tx_process
#define _TX_END_FUNC tml_tx_end
#define _TX_GET_RETRY tx_get_retry
#define _TX_GET_TID tx_get_tid
/* end define generics */

#include "tx_generic.h"

#define TML_ENTER(pop)\
tml_thread_enter(pop)

#define TML_EXIT()\
tml_thread_exit()

/* define aliases for macros */
#define TML_BEGIN _TX_BEGIN
#define TML_END _TX_END
#define TML_ONABORT _TX_ONABORT
#define TML_ONCOMMIT _TX_ONCOMMIT
#define TML_FINALLY _TX_FINALLY
#define TML_RETURN _TX_RETURN
/* end define aliases */

#define TML_WRITE_CHECK tml_tx_write()
#define TML_READ_CHECK tml_tx_read()

#define TML_WRITE_FIELD(o, field, val)\
	TML_WRITE(D_RW(o)->field, val)

#define TML_WRITE(var, val)\
	TML_WRITE_DIRECT(&(var), val, sizeof(val))

#define TML_WRITE_DIRECT(p, value, sz) ({\
	TML_WRITE_CHECK;\
	pmemobj_tx_add_range_direct(p, sz);\
	*(p) = (value);\
})

#define TML_READ_FIELD(o, field)\
	TML_READ(D_RW(o)->field)

#define TML_READ(var)\
	_TML_READ(&(var), __typeof__(var))

#define TML_READ_DIRECT(p, sz)\
	_TML_READ(p, __typeof__(*(p)))

#define _TML_READ(p, type) ({\
	type _ret = *(p);\
	TML_READ_CHECK;\
	_ret;\
})

#define TML_FREE(o)\
tml_tx_free((o).oid)

#define TML_ALLOC TX_ALLOC
#define TML_NEW TX_NEW
#define TML_ZNEW TX_ZNEW
#define TML_ZALLOC TX_ZALLOC

#define TML_ABORT()\
tml_tx_abort(0)

#define TML_RESTART tml_tx_restart
#define TML_IRREVOC tml_try_irrevoc

#endif