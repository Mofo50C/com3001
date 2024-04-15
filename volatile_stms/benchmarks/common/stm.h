#ifndef STM_H
#define STM_H 1

#if defined(NOREC)
	#include "norec.h"

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
	#include "tml.h"

	#define STM_TH_ENTER TML_ENTER
	#define STM_TH_EXIT TML_EXIT

	#define STM_STARTUP
	#define STM_CLEANUP

	#if defined(RAII)
		#define STM_BEGIN TML_BEGIN
		#define STM_END TML_END
		#define STM_ONABORT TML_ONABORT
		#define STM_ONCOMMIT TML_ONCOMMIT
		#define STM_ONRETRY TML_ONRETRY
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
#endif

#endif