// #include <stdio.h>
#include <atomic>
// #include <pthread.h>
// #include <time.h>

#define NANOSEC 1000000000.0

static std::atomic_int atom1(0);

static int glb1 = 0;
static int glb2 = 0;


// void worker(void *arg)
// {   
//     ++atom1;
// }

void worker2(void *arg)
{
    atom1.fetch_add(1);
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
