#ifndef STM_H
#define STM_H 1

#if defined(TML)
	#include <tml.h>

	#define STM_TH_ENTER TML_ENTER
	#define STM_TH_EXIT TML_EXIT

	#define STM_STARTUP
	#define STM_CLEANUP

	#if defined(RAII)
		#define STM_BEGIN TML_BEGIN
		#define STM_END TML_END
		#define STM_ONABORT TML_ONABORT
		#define STM_ONCOMMIT TML_ONCOMMIT
		#define STM_FINALLY TML_FINALLY
	#else
		#define STM_BEGIN() TML_BEGIN() {
		#define STM_END() } TML_END
	#endif

	#define STM_READ TML_READ
	#define STM_WRITE TML_WRITE

	#define STM_READ_DIRECT TML_READ_DIRECT
	#define STM_WRITE_DIRECT TML_WRITE_DIRECT

	#define STM_FREE TML_FREE
	#define STM_ALLOC TML_ALLOC
	#define STM_ZALLOC TML_ZALLOC
	#define STM_NEW TML_NEW
	#define STM_ZNEW TML_ZNEW
#elif defined(NOREC)
	#include <norec.h>

	#define STM_TH_ENTER NOREC_ENTER
	#define STM_TH_EXIT NOREC_EXIT

	#define STM_STARTUP
	#define STM_CLEANUP

	#if defined(RAII)
		#define STM_BEGIN NOREC_BEGIN
		#define STM_END NOREC_END
		#define STM_ONABORT NOREC_ONABORT
		#define STM_ONCOMMIT NOREC_ONCOMMIT
		#define STM_FINALLY NOREC_FINALLY
	#else
		#define STM_BEGIN() NOREC_BEGIN() {
		#define STM_END() } NOREC_END
	#endif

	#define STM_READ NOREC_READ
	#define STM_WRITE NOREC_WRITE

	#define STM_READ_DIRECT NOREC_READ_DIRECT
	#define STM_WRITE_DIRECT NOREC_WRITE_DIRECT

	#define STM_FREE NOREC_FREE
	#define STM_ALLOC NOREC_ALLOC
	#define STM_ZALLOC NOREC_ZALLOC
	#define STM_NEW NOREC_NEW
	#define STM_ZNEW NOREC_ZNEW
#else
	#include <txpmdk.h>

	#define STM_TH_ENTER TXPMDK_ENTER
	#define STM_TH_EXIT TXPMDK_EXIT

	#define STM_STARTUP
	#define STM_CLEANUP

	#if defined(RAII)
		#define STM_BEGIN TXPMDK_BEGIN
		#define STM_END TXPMDK_END
		#define STM_ONABORT TXPMDK_ONABORT
		#define STM_ONCOMMIT TXPMDK_ONCOMMIT
		#define STM_FINALLY TXPMDK_FINALLY
	#else
		#define STM_BEGIN() TXPMDK_BEGIN() {
		#define STM_END() } TXPMDK_END
	#endif

	#define STM_READ TXPMDK_READ
	#define STM_WRITE TXPMDK_WRITE

	#define STM_READ_DIRECT TXPMDK_READ_DIRECT
	#define STM_WRITE_DIRECT TXPMDK_WRITE_DIRECT

	#define STM_FREE TXPMDK_FREE
	#define STM_ALLOC TXPMDK_ALLOC
	#define STM_ZALLOC TXPMDK_ZALLOC
	#define STM_NEW TXPMDK_NEW
	#define STM_ZNEW TXPMDK_ZNEW
#endif

#define STM_READ_FIELD_L(o, field)\
	(D_RO(o)->field)

#define STM_READ_L(var)\
	(var)

#define STM_WRITE_FIELD_L(o, field, val)\
	D_RW(o)->field = val

#define STM_WRITE_L(var, val)\
	var = val

#endif