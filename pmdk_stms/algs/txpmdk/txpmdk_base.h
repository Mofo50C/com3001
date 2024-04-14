#include <libpmemobj.h>
#ifndef TXPMDK_BASE_H
#define TXPMDK_BASE_H 1

#ifdef __cplusplus
extern "C" {
#endif

void txpmdk_startup(PMEMobjpool *pop);

void txpmdk_cleanup(void);

int txpmdk_begin(jmp_buf env);

pid_t txpmdk_get_tid(void);

void txpmdk_preabort(void);

#ifdef __cplusplus
}
#endif

#endif