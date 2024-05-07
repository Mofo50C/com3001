#ifndef TML_BASE_H
#define TML_BASE_H 1

#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

void tml_thread_enter(void);

void tml_thread_exit(void);

int tml_tx_begin(jmp_buf env);

void tml_tx_commit(void);

void tml_tx_process(void);

int tml_tx_end(void);

void tml_try_irrevoc(void);

void tml_tx_write(void);

void tml_tx_read(void);

void tml_tx_abort(int err);

int tml_tx_free(void *ptr);

void *tml_tx_malloc(size_t size);

void *tml_tx_zalloc(size_t size);

#ifdef __cplusplus
}
#endif

#endif