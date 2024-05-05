#ifndef NOREC_BASE_H
#define NOREC_BASE_H 1

#include <stdbool.h>
#include <stdlib.h>
#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

void norec_thread_enter(PMEMobjpool *pop);

void norec_thread_exit(void);

void norec_startup(void);

void norec_shutdown(void);

int norec_tx_begin(jmp_buf env);

void norec_tx_commit(void);

void norec_tx_process(void);

int norec_tx_end(void);

void norec_tx_write(void *pdirect_field, size_t field_size, void *pval);

void norec_tx_free(PMEMoid poid);

void norec_tx_abort(void);

int norec_get_retry(void);

pid_t norec_get_tid(void);

void norec_rdset_add(void *pdirect, void *src, size_t size);

bool norec_wrset_get(void *pdirect, void *buf, size_t size);

void norec_validate(void);

int norec_prevalidate(void);

#ifdef __cplusplus
}
#endif

#endif