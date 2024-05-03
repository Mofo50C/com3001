#include <stdatomic.h>
#include <stdio.h>

#include "tx.h"

static _Atomic int total_retries = 0;
static _Atomic int total_commits = 0;

void tx_add_metrics(int retries, int commits)
{
	atomic_fetch_add_explicit(&total_retries, retries, memory_order_relaxed);
	atomic_fetch_add_explicit(&total_commits, commits, memory_order_relaxed);
}

void tx_print_metrics(void)
{
	printf("[[TM METRICS]]\n");
    printf("\tTotal retries: %d\n", total_retries);
    printf("\tTotal commits: %d\n", total_commits);
}