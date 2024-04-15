#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define RAII
#include "stm.h"

struct accounts {
	int shared_acc;
	int other_accs[];
};

struct root {
	struct accounts *accs;
};

static struct root root = {.accs = NULL};


struct transfer_args {
	int price;
	int acc_index;
};

void *process_transfer(void *arg)
{
	struct transfer_args *args = (struct transfer_args *)arg;
	int price = args->price;
	int acc_index = args->acc_index;

	pid_t tid = gettid();
	printf("p%d with tid %d\n", acc_index + 1, tid);
	struct accounts *accsp = root.accs;
	
	STM_TH_ENTER();

	STM_BEGIN() {
		int shared = STM_READ(accsp->shared_acc);
		int other = STM_READ(accsp->other_accs[acc_index]);
		STM_WRITE(accsp->shared_acc, (shared + price));
		STM_WRITE(accsp->other_accs[acc_index], (other - price));
	} STM_END

	STM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 4) {
		printf("usage: %s <num_accs> <init_balance> <price>\n", argv[0]);
		return 1;
	}

	int num_accs = atoi(argv[1]);
	int init_balance = atoi(argv[2]);
	int price = atoi(argv[3]);

	if (num_accs < 1) {
		printf("<num_accs> must be at least 1\n");
		return 1;
	}

	if (init_balance < price) {
		printf("<init_balance> should be greater than or equal to the <price>\n");
		return 1;
	}

	int i;
	int balances[num_accs];
	for (i = 0; i < num_accs; i++) {
		balances[i] = init_balance;
	}

	size_t sz = sizeof(struct accounts) + sizeof(int[num_accs]);
	struct accounts *accsp = malloc(sz);

	if (accsp == NULL)
		return 1;
	
	accsp->shared_acc = init_balance;
	memcpy(accsp->other_accs, balances, sizeof(int[num_accs]));
	root.accs = accsp;

	int total = (num_accs + 1) * init_balance;

	printf("Shared Account before: %d\n", accsp->shared_acc);
	printf("Total (%d accs) before: %d\n", num_accs + 1, total);

	pthread_t workers[num_accs];
	struct transfer_args arg_data[num_accs];

	for (i = 0; i < num_accs; i++) {
		struct transfer_args *args = &arg_data[i];
		args->acc_index = i;
		args->price = price;
		pthread_create(&workers[i], NULL, process_transfer, args);
	}

	for (i = 0; i < num_accs; i++) {
		pthread_join(workers[i], NULL);
	}

	total = accsp->shared_acc;
	for (i = 0; i < num_accs; i++) {
		total += accsp->other_accs[i];
	}
	int shared = accsp->shared_acc;

	printf("Shared Account after: %d\n", shared);
	printf("Total (%d accs) after: %d\n", num_accs + 1, total);

	free(accsp);
	root.accs = NULL;
	
	if ((shared != (init_balance + price * num_accs)) || (total != (num_accs + 1) * init_balance)) {
		printf("TX isolation failed\n");
	}

	return 0;
}
