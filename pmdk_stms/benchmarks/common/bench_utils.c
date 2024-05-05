#define _GNU_SOURCE
#include <unistd.h>

#include <stdlib.h>
#include <time.h>
#include "bench_utils.h"

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