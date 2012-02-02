#include "pubSubTM.h"

#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>



void *run(void *);

int RCCE_APP(int argc, char **argv) {

    RCCE_init(&argc, &argv);
    iRCCE_init();
    int ID = RCCE_ue();
    
    pthread_t pt;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int rc = pthread_create(&pt, NULL, run, (void *) ID);
    if (rc) {
        PRINTD("***Could not start thread");
        exit(-1);
    }

    ps_init();
   
    pthread_attr_destroy(&attr);
    pthread_join(pt, NULL);
    printf("%d: joined thread\n", ID);
    RCCE_finalize();
    pthread_exit(NULL);
    RCCE_barrier(&RCCE_COMM_WORLD);
    PRINTD("\t\t\t\t\t\t\t>>    CLEARED BARRIER!!<< ");
    RCCE_finalize();
    return (0);
}

void *run(void *data) {
    double wait_time = 0.3 + 0.033*(ID + 1);
    double sent_last = RCCE_wtime();
    double totaltime = 0;
    long long int i = 1;

    while (i < 30) {
        double now = RCCE_wtime();
        if (now - sent_last > wait_time) {
            sent_last = now;
            i++;

            int target;
            do {
                target = rand() % RCCE_num_ues();
            } while (target == RCCE_ue());

            //PRINTD("atemmpt..");
            double start = RCCE_wtime();
            ps_subscribe(target);
            ps_publish(target, 0);
            double diff = RCCE_wtime() - start;
            totaltime += diff;
            PRINTD("\t\t\t\t %lld|completed for %d in %f", (i - 1), target, diff);
        }
    }

    PRINTD("\t\t\t\t\t\t\t>>completed in avg: %f sec.", totaltime/(i-1));

    pthread_exit(NULL);
}

