#ifndef DTML_BASE_H
#define DTML_BASE_H 1

#include <stdlib.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

enum tx_stage {
    DTML_STAGE_NONE,
    DTML_STAGE_WORK,
    DTML_STAGE_ONRETRY,
    DTML_STAGE_ONABORT,
    DTML_STAGE_ONCOMMIT,
    DTML_STAGE_FINALLY
};

enum tx_stage tx_get_stage(void);

int tx_get_retry(void);

pid_t tx_get_tid(void);

int tx_get_error(void);

void tx_abort(int errnum);

void tx_restart(void);

int tx_get_level(void);

void dtml_thread_enter(void);

void dtml_thread_exit(void);

int dtml_tx_begin(jmp_buf env);

void dtml_tx_commit(void);

void dtml_tx_process(void);

int dtml_tx_end(void);

void dtml_tx_write(void);

void dtml_tx_read(void);

int dtml_tx_free(void *ptr);

void *dtml_tx_malloc(size_t size, int fill);

#ifdef __cplusplus
}
#endif

#endif