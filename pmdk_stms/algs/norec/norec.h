#ifndef NOREC_H
#define NOREC_H 1

#include "norec_base.h"

/* define stm-specific functions */
#define _TX_BEGIN_FUNC norec_tx_begin
#define _TX_PROCESS_FUNC norec_tx_process
#define _TX_END_FUNC norec_tx_end
#define _TX_GET_RETRY norec_get_retry
#define _TX_GET_TID norec_get_tid
#define _TX_PREABORT norec_preabort
/* end define generics */

#include "tx_generic.h"

#define NOREC_STARTUP(pop)\
norec_tx_startup(pop)

#define NOREC_CLEANUP()\
norec_tx_cleanup()

/* define aliases for macros */
#define NOREC_BEGIN _TX_BEGIN
#define NOREC_END _TX_END
#define NOREC_ONABORT _TX_ONABORT
#define NOREC_ONCOMMIT _TX_ONCOMMIT
#define NOREC_FINALLY _TX_FINALLY
/* end define aliases */

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

#define NOREC_FREE TX_FREE
#define NOREC_ALLOC TX_ALLOC
#define NOREC_NEW TX_NEW
#define NOREC_ZNEW TX_ZNEW
#define NOREC_ZALLOC TX_ZALLOC

#endif