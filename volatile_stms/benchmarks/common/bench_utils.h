#ifndef BENCH_UTILS_H
#define BENCH_UTILS_H 1

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

void clamp_cpu(int cpu);

int msleep(int msec);

double get_elapsed_time(struct timespec *start, struct timespec *finish);

#ifdef __cplusplus
}
#endif

#endif 