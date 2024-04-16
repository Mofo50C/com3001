#ifndef STM_H
#define STM_H 1

#include "dtml.h"

#define STM_TH_ENTER DTML_ENTER
#define STM_TH_EXIT DTML_EXIT

#define STM_STARTUP
#define STM_CLEANUP

#if defined(RAII)
	#define STM_BEGIN DTML_BEGIN
	#define STM_END DTML_END
	#define STM_ONABORT DTML_ONABORT
	#define STM_ONCOMMIT DTML_ONCOMMIT
	#define STM_ONRETRY DTML_ONRETRY
	#define STM_FINALLY DTML_FINALLY
#else
	#define STM_BEGIN() DTML_BEGIN() {
	#define STM_END() } DTML_END
#endif

#define STM_READ DTML_READ
#define STM_WRITE DTML_WRITE

#define STM_READ_DIRECT DTML_READ_DIRECT
#define STM_WRITE_DIRECT DTML_WRITE_DIRECT

#define STM_FREE DTML_FREE
#define STM_ALLOC DTML_ALLOC
#define STM_ZALLOC DTML_ZALLOC
#define STM_NEW DTML_NEW
#define STM_ZNEW DTML_ZNEW

#endif