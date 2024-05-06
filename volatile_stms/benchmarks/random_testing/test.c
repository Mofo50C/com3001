#define _GNU_SOURCE
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#define RAII
#include "stm.h"


int main(int argc, char const *argv[])
{
    STM_TH_ENTER();

    printf("before tx\n");
    
    int flag = 0;
    STM_BEGIN() {
        printf("begin outer\n");
        STM_BEGIN() {
            printf("begin inner\n");
            // STM_ABORT();
            if (!flag) {
                flag++;
                STM_RESTART();
            }
            printf("should only print once...\n");

            STM_RETURN();
            printf("skip this\n");
        } STM_ONABORT {
            printf("okay now we abort\n");
        } STM_ONCOMMIT {
            printf("committing inner...\n");
        } STM_FINALLY {
            printf("inner finally...always called\n");
        } STM_END

        printf("after inner!\n");

        STM_RETURN();

        printf("lolololol\n");
    } STM_ONABORT {
        printf("waterfall to the next abort nice!\n");
    } STM_ONCOMMIT {
        printf("IM COOOMMIIITTTTING!!!\n");
    } STM_FINALLY {
        printf("always called\n");
    } STM_END

    printf("after tx\n");

    STM_TH_EXIT();

    return 0;
}
