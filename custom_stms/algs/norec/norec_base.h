#ifndef NOREC_BASE_H
#define NOREC_BASE_H 1

#include <stdbool.h>
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

void norec_thread_enter(void);

void norec_thread_exit(void);

int norec_tx_begin(jmp_buf env);

void norec_rdset_add(void *pdirect, void *src, size_t size);

bool norec_wrset_get(void *pdirect, void *buf, size_t size);

void norec_validate(void);

bool norec_prevalidate(void);

void norec_tx_write(void *pdirect_field, size_t field_size, void *pval);

void norec_tx_commit(void);

void norec_tx_process(void);

int norec_tx_end(void);

int norec_tx_free(void *ptr);

void *norec_tx_malloc(size_t size, int fill);

#ifdef __cplusplus
}
#endif

#endif