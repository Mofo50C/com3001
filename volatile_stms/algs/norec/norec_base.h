#ifndef NOREC_BASE_H
#define NOREC_BASE_H 1

#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

void norec_thread_enter(void);

void norec_thread_exit(void);

int norec_tx_begin(jmp_buf env);

void norec_rdset_add(void *pdirect, void *buf, size_t size);

bool norec_wrset_get(void *pdirect, void *retbuf, size_t size);

void norec_validate(void);

bool norec_prevalidate(void);

void norec_tx_write(void *pdirect_field, size_t field_size, void *buf);

void norec_tx_commit(void);

void norec_tx_abort(int err);

void norec_try_irrevoc(void);

int norec_isirrevoc(void);

void norec_tx_process(void);

int norec_tx_end(void);

int norec_tx_free(void *ptr);

void *norec_tx_malloc(size_t size);

void *norec_tx_zalloc(size_t size);

#ifdef __cplusplus
}
#endif

#endif