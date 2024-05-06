#ifndef TML_BASE_H
#define TML_BASE_H 1

#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

void tml_startup(void);

void tml_shutdown(void);

void tml_thread_enter(PMEMobjpool *pop);

void tml_thread_exit(void);

int tml_tx_begin(jmp_buf env);

void tml_tx_commit(void);

void tml_tx_process(void);

int tml_tx_end(void);

void tml_tx_write(void);

void tml_tx_free(PMEMoid poid);

void tml_tx_restart(void);

void tml_tx_abort(void);

void tml_tx_read(void);

int tml_get_retry(void);

pid_t tml_get_tid(void);

#ifdef __cplusplus
}
#endif

#endif