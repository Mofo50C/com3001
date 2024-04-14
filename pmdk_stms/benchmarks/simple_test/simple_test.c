#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <libpmemobj.h>

#define RAII
#include "stm.h"

POBJ_LAYOUT_BEGIN(simple_test);
POBJ_LAYOUT_ROOT(simple_test, struct root);
POBJ_LAYOUT_TOID(simple_test, struct accounts);
POBJ_LAYOUT_END(simple_test);

struct accounts {
	int shared_acc;
	int other_accs[];
};

struct root {
	TOID(struct accounts) accs;
};

static PMEMobjpool *pop;

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
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	struct accounts *accsp = D_RW(D_RW(root)->accs);
	
	STM_TH_ENTER(pop);

	STM_BEGIN() {
		int shared, other;
		shared = STM_READ(accsp->shared_acc);
		int *pother = (accsp->other_accs + acc_index);
		other = STM_READ_DIRECT(pother, sizeof(int));
		STM_WRITE(accsp->shared_acc, (shared + price));
		STM_WRITE_DIRECT(pother, (other - price), sizeof(int));
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

	const char *path = "/mnt/pmem/simple_test";
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(simple_test),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(simple_test))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

	int i;
	int balances[num_accs];
	for (i = 0; i < num_accs; i++) {
		balances[i] = init_balance;
	}

	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	struct root *rootp = D_RW(root);

	TX_BEGIN(pop) {
		TOID(struct accounts) accs = TX_ZALLOC(struct accounts, sizeof(struct accounts) + sizeof(int[num_accs]));
		D_RW(accs)->shared_acc = init_balance;
		memcpy(D_RW(accs)->other_accs, balances, sizeof(int[num_accs]));
		TX_ADD(root);
		D_RW(root)->accs = accs;
	} TX_END

	int total = (num_accs + 1) * init_balance;
	struct accounts *accsp = D_RW(rootp->accs);

	printf("Shared Account before: %d\n", accsp->shared_acc);
	printf("Total before: %d\n", total);

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
	printf("Total after: %d\n", total);

	TX_BEGIN(pop) {
		TX_ADD(root);
		TX_FREE(D_RW(root)->accs);
		D_RW(root)->accs = TOID_NULL(struct accounts);
	} TX_ONABORT {
		printf("abort!\n");
	} TX_END;

	pmemobj_close(pop);
	
	if ((shared != (init_balance + price * num_accs)) || (total != (num_accs + 1) * init_balance)) {
		printf("TX isolation failed\n");
	}

	return 0;
}
