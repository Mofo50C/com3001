#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <libpmemobj.h>

#define RAII
#include "ptm.h"

POBJ_LAYOUT_BEGIN(simple_test);
POBJ_LAYOUT_ROOT(simple_test, struct root);
POBJ_LAYOUT_END(simple_test);

static PMEMobjpool *pop;

struct root {
};

int main(int argc, char const *argv[])
{
    const char *path = "/mnt/pmem/random_testing";
	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(random_testing),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(random_testing))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

    PTM_TH_ENTER(pop);

    printf("before tx\n");
    
    int flag = 0;
    PTM_BEGIN() {
        printf("begin outer\n");
        PTM_BEGIN() {
            printf("begin inner\n");
            // PTM_ABORT();
            if (!flag) {
                flag++;
                PTM_RESTART();
            }
            printf("should only print once...\n");

            PTM_RETURN();
            printf("skip this\n");
        } PTM_ONABORT {
            printf("okay now we abort\n");
        } PTM_ONCOMMIT {
            printf("committing inner...\n");
        } PTM_FINALLY {
            printf("inner finally...always called\n");
        } PTM_END

        printf("after inner!\n");

        PTM_RETURN();

        printf("lolololol\n");
    } PTM_ONABORT {
        printf("waterfall to the next abort nice!\n");
    } PTM_ONCOMMIT {
        printf("IM COOOMMIIITTTTING!!!\n");
    } PTM_FINALLY {
        printf("always called\n");
    } PTM_END

    printf("after tx\n");

    PTM_TH_EXIT();

    pmemobj_close(pop);

    return 0;
}
