#ifndef TX_H
#define TX_H 1

#include <libpmemobj.h>

#ifdef __cplusplus
extern "C" {
#endif

void tx_add_metrics(int retries, int commits);

void tx_print_metrics(void);

int tx_get_retry(void);

pid_t tx_get_tid(void);

int tx_get_level(void);

int tx_begin(jmp_buf env);

int tx_end(void (*end_cb)(void));

void tx_process(void (*commit_cb)(void));

void tx_thread_enter(PMEMobjpool *pop);

void tx_thread_exit(void);

void tx_startup(void);

void tx_shutdown(void);

void tx_restart(void);

#ifdef __cplusplus
}
#endif

#endif