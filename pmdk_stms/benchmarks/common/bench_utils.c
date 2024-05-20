#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include "bench_utils.h"

#define NANOSEC 1000000000.0

int msleep(int msec)
{
    struct timespec ts;
    int res;

    if (msec <= 0)
    {
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res);

    return res;
}

double get_elapsed_time(struct timespec *start, struct timespec *finish)
{
    double elapsed = (finish->tv_sec - start->tv_sec);
    elapsed += (finish->tv_nsec - start->tv_nsec) / NANOSEC;

    return elapsed;
}

void clamp_cpu(int cpu)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	sched_setaffinity(0, sizeof(cpuset), &cpuset);
}