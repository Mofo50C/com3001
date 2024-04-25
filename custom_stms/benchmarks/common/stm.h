#ifndef STM_H
#define STM_H 1

#if defined(DTML)
	#include <dtml.h>

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
#endif

#endif