#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <libpmemobj.h>

#define RAII
#include "ptm.h"
#include "bench_utils.h"

POBJ_LAYOUT_BEGIN(simple_test);
POBJ_LAYOUT_ROOT(simple_test, struct root);
POBJ_LAYOUT_TOID(simple_test, struct accounts);
POBJ_LAYOUT_END(simple_test);

struct accounts {
	int balance1;
	int balance2;
};

struct root {
	TOID(struct accounts) accs;
};

static PMEMobjpool *pop;

struct transfer_args {
	int idx;
	int price;
	int msec;
};

void *process_simple(void *arg)
{
	struct transfer_args *args = (struct transfer_args *)arg;
	int price = args->price;

	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	struct accounts *accsp = D_RW(D_RW(root)->accs);
	
	PTM_TH_ENTER(pop);

	PTM_BEGIN() {
		int balance1 = PTM_READ(accsp->balance1);
		int balance2 = PTM_READ(accsp->balance2);
		msleep(args->msec);
		PTM_WRITE(accsp->balance1, (balance1 + price));
		PTM_WRITE(accsp->balance2, (balance2 - price));
	} PTM_ONABORT {
		DEBUGABORT();
	} PTM_END

	PTM_TH_EXIT();

	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc < 6) {
		printf("usage: %s <pool_file> <num_threads> <balance1> <balance2> <price> [sleep]\n", argv[0]);
		return 1;
	}

	int num_threads = atoi(argv[2]);
	int balance1 = atoi(argv[3]);
	int balance2 = atoi(argv[4]);
	int price = atoi(argv[5]);
	int msec = 0;
	if (argc == 7)
		msec = atoi(argv[6]);

	if (num_threads < 1) {
		printf("<num_threads> must be at least 1\n");
		return 1;
	}

	if (balance2 - num_threads * price < 0) {
		printf("<balance2> must be equal to at least <num_threads> * <price>\n");
		return 1;
	}

	const char *path = argv[1];
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

	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	struct root *rootp = D_RW(root);

	int err = 0;
	TX_BEGIN(pop) {
		TOID(struct accounts) accs = TX_ZALLOC(struct accounts, sizeof(struct accounts));
		D_RW(accs)->balance1 = balance1;
		D_RW(accs)->balance2 = balance2;
		TX_ADD(root);
		D_RW(root)->accs = accs;
	} TX_ONABORT {
		err = 1;
	} TX_END

	if (err) {
		pmemobj_close(pop);
		return 1;
	}

	struct accounts *accsp = D_RW(rootp->accs);

	printf("Balance 1 before: %d\n", accsp->balance1);
	printf("Balance 2 before: %d\n", accsp->balance2);

	pthread_t workers[num_threads];
	struct transfer_args arg_data[num_threads];

	int i;
	for (i = 0; i < num_threads; i++) {
		struct transfer_args *args = &arg_data[i];
		args->idx = i;
		args->price = price;
		args->msec = msec;
		pthread_create(&workers[i], NULL, process_simple, args);
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
	}

	printf("Balance 1 after: %d\n", accsp->balance1);
	printf("Balance 2 after: %d\n", accsp->balance2);

	if ((accsp->balance1 != balance1 + price * num_threads) && (accsp->balance2 == balance2 - price * num_threads)) {
		printf("TX isolation failed\n");
	}

	TX_BEGIN(pop) {
		TX_ADD(root);
		TX_FREE(D_RW(root)->accs);
		D_RW(root)->accs = TOID_NULL(struct accounts);
	} TX_ONABORT {
		printf("abort!\n");
	} TX_END;

	pmemobj_close(pop);
	return 0;
}
