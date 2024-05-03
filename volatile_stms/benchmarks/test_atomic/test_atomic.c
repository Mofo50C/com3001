#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>

#define NANOSEC 1000000000.0

static _Atomic int atom1 = 0;
static _Atomic int atom2 = 0;

static int glb1 = 0;
static int glb2 = 0;


void *worker(void *arg)
{   
    ++atom1;

    return NULL;
}

void *worker2(void *arg)
{
    atomic_fetch_add(&atom1, 1);
    
    return NULL;
}

int main(int argc, char const *argv[])
{
    // int num_threads = 1000;

	// int i;
    // struct timespec s, f;
	// clock_gettime(CLOCK_MONOTONIC, &s);
	// pthread_t workers[num_threads];
	// for (i = 0; i < num_threads; i++) {
    //     pthread_create(&workers[i], NULL, worker, NULL);
	// }

	// for (i = 0; i < num_threads; i++) {
	// 	pthread_join(workers[i], NULL);
	// }

    // clock_gettime(CLOCK_MONOTONIC, &f);
	// double elapsed_time = (f.tv_sec - s.tv_sec);
	// elapsed_time += (f.tv_nsec - s.tv_nsec) / NANOSEC;
	// printf("Elapsed: %f\n", elapsed_time);

    // printf("Non-atomic: %d\n", glb1);
    // printf("Atomic: %d\n", atom1);
    // printf("\nFetch add:\n");
    // printf("Non-atomic: %d\n", glb2);
    // printf("Atomic: %d\n", atom2);
    return 0;
}
