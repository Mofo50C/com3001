#ifndef PTM_H
#define PTM_H 1

#if defined(TXPMDK)
	#include <txpmdk.h>

	#define PTM_TH_ENTER TXPMDK_ENTER
	#define PTM_TH_EXIT TXPMDK_EXIT

	#define PTM_STARTUP()
	#define PTM_SHUTDOWN()

	#if defined(RAII)
		#define PTM_BEGIN TXPMDK_BEGIN
		#define PTM_END TXPMDK_END
		#define PTM_ONABORT TXPMDK_ONABORT
		#define PTM_ONCOMMIT TXPMDK_ONCOMMIT
		#define PTM_FINALLY TXPMDK_FINALLY
	#else
		#define PTM_BEGIN() TXPMDK_BEGIN() {
		#define PTM_END() } TXPMDK_END
	#endif

	#define PTM_READ_FIELD TXPMDK_READ_FIELD
	#define PTM_WRITE_FIELD TXPMDK_WRITE_FIELD

	#define PTM_READ TXPMDK_READ
	#define PTM_WRITE TXPMDK_WRITE

	#define PTM_READ_DIRECT TXPMDK_READ_DIRECT
	#define PTM_WRITE_DIRECT TXPMDK_WRITE_DIRECT

	#define PTM_FREE TXPMDK_FREE
	#define PTM_ALLOC TXPMDK_ALLOC
	#define PTM_ZALLOC TXPMDK_ZALLOC
	#define PTM_NEW TXPMDK_NEW
	#define PTM_ZNEW TXPMDK_ZNEW

	#define PTM_ABORT TXPMDK_ABORT
	#define PTM_RETURN TXPMDK_RETURN
	#define PTM_RESTART
	#define PTM_IRREVOC
#elif defined(NOREC)
	#include <norec.h>

	#define PTM_TH_ENTER NOREC_ENTER
	#define PTM_TH_EXIT NOREC_EXIT

	#define PTM_STARTUP tx_startup
	#define PTM_SHUTDOWN tx_shutdown

	#if defined(RAII)
		#define PTM_BEGIN NOREC_BEGIN
		#define PTM_END NOREC_END
		#define PTM_ONABORT NOREC_ONABORT
		#define PTM_ONCOMMIT NOREC_ONCOMMIT
		#define PTM_FINALLY NOREC_FINALLY
	#else
		#define PTM_BEGIN() NOREC_BEGIN() {
		#define PTM_END() } NOREC_END
	#endif

	#define PTM_READ_FIELD NOREC_READ_FIELD
	#define PTM_WRITE_FIELD NOREC_WRITE_FIELD

	#define PTM_READ NOREC_READ
	#define PTM_WRITE NOREC_WRITE

	#define PTM_READ_DIRECT NOREC_READ_DIRECT
	#define PTM_WRITE_DIRECT NOREC_WRITE_DIRECT

	#define PTM_FREE NOREC_FREE
	#define PTM_ALLOC NOREC_ALLOC
	#define PTM_ZALLOC NOREC_ZALLOC
	#define PTM_NEW NOREC_NEW
	#define PTM_ZNEW NOREC_ZNEW

	#define PTM_ABORT NOREC_ABORT
	#define PTM_RETURN NOREC_RETURN
	#define PTM_RESTART NOREC_RESTART
	#define PTM_IRREVOC NOREC_IRREVOC
#else
	#include <tml.h>

	#define PTM_TH_ENTER TML_ENTER
	#define PTM_TH_EXIT TML_EXIT

	#define PTM_STARTUP tx_startup
	#define PTM_SHUTDOWN tx_shutdown

	#if defined(RAII)
		#define PTM_BEGIN TML_BEGIN
		#define PTM_END TML_END
		#define PTM_ONABORT TML_ONABORT
		#define PTM_ONCOMMIT TML_ONCOMMIT
		#define PTM_FINALLY TML_FINALLY
	#else
		#define PTM_BEGIN() TML_BEGIN() {
		#define PTM_END() } TML_END
	#endif

	#define PTM_READ_FIELD TML_READ_FIELD
	#define PTM_WRITE_FIELD TML_WRITE_FIELD

	#define PTM_READ TML_READ
	#define PTM_WRITE TML_WRITE

	#define PTM_READ_DIRECT TML_READ_DIRECT
	#define PTM_WRITE_DIRECT TML_WRITE_DIRECT

	#define PTM_FREE TML_FREE
	#define PTM_ALLOC TML_ALLOC
	#define PTM_ZALLOC TML_ZALLOC
	#define PTM_NEW TML_NEW
	#define PTM_ZNEW TML_ZNEW

	#define PTM_ABORT TML_ABORT
	#define PTM_RETURN TML_RETURN
	#define PTM_RESTART TML_RESTART
	#define PTM_IRREVOC TML_IRREVOC
#endif

#define PTM_READ_FIELD_L(o, field)\
	(D_RO(o)->field)

#define PTM_WRITE_FIELD_L(o, field, val)\
	D_RW(o)->field = (val)

#define PTM_READ_L(var)\
	(var)

#define PTM_WRITE_L(var, val)\
	var = (val)

#define PTM_READ_DIRECT_L(ptr)\
	*(ptr)

#define PTM_WRITE_DIRECT_L(ptr, val)\
	*(ptr) = (val)

#endif