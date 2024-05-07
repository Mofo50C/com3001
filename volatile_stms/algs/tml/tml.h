#ifndef TML_H
#define TML_H 1

#include <errno.h>
#include "tx.h"
#include "util.h"
#include "tml_base.h"

#define _TX_BEGIN_FUNC tml_tx_begin
#define _TX_PROCESS_FUNC tml_tx_process
#define _TX_END_FUNC tml_tx_end

#include "tx_generic.h"

/* define aliases for macros */
#define TML_BEGIN _TX_BEGIN
#define TML_END _TX_END
#define TML_ONABORT _TX_ONABORT
#define TML_ONCOMMIT _TX_ONCOMMIT
#define TML_FINALLY _TX_FINALLY
#define TML_ONRETRY _TX_ONRETRY
#define TML_RETURN _TX_RETURN
/* end define aliases */

#define TML_ENTER tml_thread_enter
#define TML_EXIT tml_thread_exit

#define TML_WRITE_CHECK tml_tx_write()
#define TML_READ_CHECK tml_tx_read()

#define TML_WRITE_FIELD(o, field, val)\
	TML_WRITE(o->field, val)

#define TML_WRITE(var, val)\
	TML_WRITE_DIRECT(&(var), val, sizeof(val))

#define TML_WRITE_DIRECT(p, value, sz) ({\
	TML_WRITE_CHECK;\
	*(p) = (value);\
})

#define TML_READ_FIELD(o, field)\
	TML_READ(o->field)

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
tml_tx_malloc(sz)

#define TML_ZALLOC(sz)\
tml_tx_zalloc(sz)

#define TML_NEW(type)\
tml_tx_malloc(sizeof(type))

#define TML_ZNEW(type)\
tml_tx_zalloc(sizeof(type))

#define TML_ABORT()\
tml_tx_abort(0);

#define TML_RESTART tx_restart
#define TML_IRREVOC tml_try_irrevoc

#endif