#ifndef TXPMDK_H
#define TXPMDK_H 1

#include "txpmdk_base.h"

/* define the generic macros with current stm-specific functions */
#define _TX_BEGIN_FUNC txpmdk_begin
#define _TX_PROCESS_FUNC pmemobj_tx_process
#define _TX_END_FUNC pmemobj_tx_end
#define _TX_GET_TID txpmdk_get_tid
#define _TX_GET_RETRY txpmdk_get_retry
/* end define generics */

#include "tx_generic.h"

#define TXPMDK_ENTER txpmdk_thread_enter
#define TXPMDK_EXIT txpmdk_thread_exit

/* define aliases for macros */
#define TXPMDK_BEGIN _TX_BEGIN
#define TXPMDK_END _TX_END
#define TXPMDK_ONABORT _TX_ONABORT
#define TXPMDK_ONCOMMIT _TX_ONCOMMIT
#define TXPMDK_FINALLY _TX_FINALLY
/* end define aliases */

#define TXPMDK_READ_FIELD(o, field) (D_RO(o)->field)
#define TXPMDK_WRITE_FIELD(o, field, val) ({ TX_ADD_FIELD(o, field); D_RW(o)->field = val; })

#define TXPMDK_READ(var) (var)
#define TXPMDK_WRITE(var, val) ({ TX_ADD_DIRECT(&(var)); var = val; })

#define TXPMDK_READ_DIRECT(p, sz) (*(p))
#define TXPMDK_WRITE_DIRECT(p, val, sz) ({ pmemobj_tx_add_range_direct(p, sz); *(p) = val; })

#define TXPMDK_FREE TX_FREE
#define TXPMDK_ALLOC TX_ALLOC
#define TXPMDK_NEW TX_NEW
#define TXPMDK_ZNEW TX_ZNEW
#define TXPMDK_ZALLOC TX_ZALLOC

#define TXPMDK_ABORT()\
pmemobj_tx_abort(0)

#endif