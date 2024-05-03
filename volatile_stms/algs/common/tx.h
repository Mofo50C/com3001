#ifndef TX_H
#define TX_H 1

#include <setjmp.h>
#include "tx_util.h"

#ifdef __cplusplus
extern "C" {
#endif

enum tx_stage {
    TX_STAGE_NONE,
    TX_STAGE_WORK,
    TX_STAGE_ONRETRY,
    TX_STAGE_ONABORT,
    TX_STAGE_ONCOMMIT,
    TX_STAGE_FINALLY
};

enum tx_stage tx_get_stage(void);

int tx_get_retry(void);

pid_t tx_get_tid(void);

int tx_get_error(void);

void tx_abort(int errnum);

void tx_restart(void);

int tx_get_level(void);

void tx_thread_enter(void);

void tx_thread_exit(void);

void tx_startup(void);

void tx_shutdown(void);

int tx_begin(jmp_buf env);

void *tx_malloc(size_t size, int zero);

int tx_free(void *ptr);

void tx_process(void (*commit_cb)(void));

int tx_end(void (*end_cb)(void));

void tx_commit(void);

void tx_reclaim_frees(void);

#ifdef __cplusplus
}
#endif

#endif