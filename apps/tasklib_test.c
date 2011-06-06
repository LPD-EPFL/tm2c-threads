/*
 * File:   test_libtask.c
 * Author: trigonak
 *
 * Created on March 10, 2011, 7:43 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include "pubSubTM.h"


void run(void *arg);

/*
 *
 */
MAIN(int argc, char *argv[]) {

    RCCE_init(&argc, &argv);

    char *name = (char *) malloc(100);

    name = (char *) taskgetname();

    PRINTD("\t\tCreating task..");

    taskcreate(run, (void *) 10, 10240);

    taskdelay(1200);

    int target = (RCCE_ue() + 1)%NUM_UES;//c
     int wait_time = 1000 + 3 * (ID + 1);

    int i = 300;
    while (i-- > 0) {

        taskudelay(wait_time);

        PRINTD("\t\tpublishing %d", target);

        ps_publish(target, 0);

    }
    PRINTD("\t\texiting..");
    

    taskexit(0);

}

void run(void* arg) {
    
    ps_init();

    taskexit(0);

}
