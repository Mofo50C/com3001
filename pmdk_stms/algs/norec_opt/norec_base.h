#ifndef NOREC_BASE_H
#define NOREC_BASE_H 1

#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

void norec_tx_startup(PMEMobjpool *pop);

void norec_tx_cleanup(void);

int norec_tx_begin(jmp_buf env);

void norec_tx_commit(void);

void norec_tx_process(void);

int norec_tx_end(void);

void norec_tx_write(void *pdirect_field, size_t field_size, void *pval);

void norec_tx_abort(void);

int norec_get_retry(void);

pid_t norec_get_tid(void);

void norec_rdset_add(void *pdirect, void *src, size_t size);

int norec_wrset_get(void *pdirect, void *buf, size_t size);

void norec_validate(void);

int norec_prevalidate(void);

void norec_preabort(void);

#ifdef __cplusplus
}
#endif

#endif