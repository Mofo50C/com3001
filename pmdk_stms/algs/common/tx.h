#ifndef TX_H
#define TX_H 1

#ifdef __cplusplus
extern "C" {
#endif

void tx_add_metrics(int retries, int commits);

void tx_print_metrics(void);

#ifdef __cplusplus
}
#endif

#endif