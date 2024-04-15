#ifndef TML_BASE_H
#define TML_BASE_H 1

#include <setjmp.h>
#include <unistd.h>
#include <sys/types.h>

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

int tx_get_error(void);

int tx_get_retry(void);

pid_t tx_get_tid(void);

enum tx_stage tx_get_stage(void);

void tml_thread_enter(void);

void tml_thread_exit(void);

int tml_tx_begin(jmp_buf env);

void tml_tx_commit(void);

void tml_tx_process(void);

int tml_tx_end(void);

void tml_tx_write(void);

void tml_tx_read(void);

int tml_tx_free(void *ptr);

void *tml_tx_malloc(size_t size, int fill);

#ifdef __cplusplus
}
#endif

#endif