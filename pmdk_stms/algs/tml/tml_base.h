#include <libpmemobj.h>
#ifndef TML_BASE_H
#define TML_BASE_H 1

#ifdef __cplusplus
extern "C" {
#endif

void tml_tx_startup(PMEMobjpool *pop);

void tml_tx_cleanup(void);

int tml_tx_begin(jmp_buf env);

void tml_tx_commit(void);

void tml_tx_process(void);

int tml_tx_end(void);

void tml_tx_write(void);

void tml_tx_read(void);

int tml_get_retry(void);

pid_t tml_get_tid(void);

void tml_preabort(void);

#ifdef __cplusplus
}
#endif

#endif