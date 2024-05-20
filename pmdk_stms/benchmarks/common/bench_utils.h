#ifndef BENCH_UTILS_H
#define BENCH_UTILS_H 1

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

int msleep(int msec);

double get_elapsed_time(struct timespec *start, struct timespec *finish);

void clamp_cpu(int cpu);

#ifdef __cplusplus
}
#endif

#endif 