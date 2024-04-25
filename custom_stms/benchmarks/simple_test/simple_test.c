#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#define RAII
#include "stm.h"

struct account {
	int balance1;
	int balance2;
};

struct root {
	struct account *acc;
};

static struct root root = {.acc = NULL};


struct transfer_args {
	int idx;
	int price;
};

void *process_simple(void *arg)
{
	struct transfer_args *args = (struct transfer_args *)arg;
	int price = args->price;

	struct account *accp = root.acc;

	int acc1 = accp->balance1;
	int acc2 = accp->balance2;
	sleep(1);
	accp->balance1 = acc1 + price;
	accp->balance2 = acc2 - price;

	return NULL;
}

void *process_simple_tm(void *arg)
{
	struct transfer_args *args = (struct transfer_args *)arg;
	int price = args->price;

	struct account *accp = root.acc;

	STM_TH_ENTER();
	
	STM_BEGIN() {
		int acc1 = STM_READ(accp->balance1);
		int acc2 = STM_READ(accp->balance2);
		sleep(1);
		STM_WRITE(accp->balance1, (acc1 + price));
		STM_WRITE(accp->balance2, (acc2 - price));
	} STM_END
	
	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 5) {
		printf("usage: %s <num_threads> <balance1> <balance2> <price>\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[1]);
	int balance1 = atoi(argv[2]);
	int balance2 = atoi(argv[3]);
	int price = atoi(argv[4]);

	if (num_threads < 1) {
		printf("<num_threads> must be at least 1\n");
		return 1;
	}

	if (balance2 - num_threads * price < 0) {
		printf("<balance2> must be equal to at least <num_threads> * <price>\n");
		return 1;
	}

	int i;
	struct account *accp = malloc(sizeof(struct account));

	if (accp == NULL)
		return 1;
	
	accp->balance1 = balance1;
	accp->balance2 = balance2;
	root.acc = accp;

	printf("Balance 1 before: %d\n", accp->balance1);
	printf("Balance 2 before: %d\n", accp->balance2);

	pthread_t workers[num_threads];
	struct transfer_args arg_data[num_threads];

	for (i = 0; i < num_threads; i++) {
		struct transfer_args *args = &arg_data[i];
		args->price = price;
		// pthread_create(&workers[i], NULL, process_simple, args);
		pthread_create(&workers[i], NULL, process_simple_tm, args);
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	printf("Balance 1 after: %d\n", accp->balance1);
	printf("Balance 2 after: %d\n", accp->balance2);

	assert(accp->balance1 == balance1 + price * num_threads);
	assert(accp->balance2 == balance2 - price * num_threads);

	free(accp);
	
	return 0;
}
