#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "bench_utils.h"

#define RAII
#include "stm.h"

struct root {
    int n;

};

static struct root root = {.n = 0};

void *worker1(void *arg)
{

    STM_TH_ENTER();

    STM_BEGIN() {
        msleep(500);
        STM_WRITE(root.n, 1);
    } STM_END

    STM_TH_EXIT();

    return NULL;
}

void *worker2(void *arg)
{

    STM_TH_ENTER();

    STM_BEGIN() {
        msleep(500);
        int n = STM_READ(root.n);
        printf("n: %d\n", n);
    } STM_END

    STM_TH_EXIT();

    return NULL;
}


int main(int argc, char const *argv[])
{
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker1, NULL);
    pthread_create(&t2, NULL, worker2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
