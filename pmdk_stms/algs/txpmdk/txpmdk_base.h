#ifndef TXPMDK_BASE_H
#define TXPMDK_BASE_H 1
#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

void txpmdk_thread_enter(PMEMobjpool *pop);

void txpmdk_thread_exit(void);

int txpmdk_begin(jmp_buf env);

pid_t txpmdk_get_tid(void);

int txpmdk_get_retry(void);

#ifdef __cplusplus
}
#endif

#endif