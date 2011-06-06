/*working
 */

#include "pubSubTM.h"

void run(void *);

TASKMAIN(int argc, char **argv) {

    RCCE_init(&argc, &argv);
    ps_init_();

    int ID = RCCE_ue();


    taskudelay(3);

    double totaltime = 0;
    if (ID > 0) {
        double start = RCCE_wtime();
/*
        if (ps_subscribe(0)) {
            PRINTD("subscribed to addr 0");
        }
        else {
            PRINTD("NOT subscribed to addr 0");
        }
*/
        ps_subscribe(0);


        double diff = RCCE_wtime() - start;
        totaltime += diff;
        PRINTD("\t\t\t\t |completed in %f", diff);
    }
    
    

/*
    int wait_time = 10 + 3 * (ID + 1);

    double totaltime = 0;
    long long int i = 1;

    while (i < 2000) {
        taskudelay(wait_time);
        i++;

        int target;
        do {
            target = rand() % RCCE_num_ues();
        } while (target == RCCE_ue());


        double start = RCCE_wtime();
        ps_publish(target);
        ps_subscribe(target);

        double diff = RCCE_wtime() - start;
        totaltime += diff;
        //PRINTD("\t\t\t\t %lld|completed for %d in %f", (i - 1), target, diff);


    }

    PRINTD("\t\t\t\t\t\t\t>>completed in avg: %f sec.", totaltime / (i - 1));
*/
    taskexit(0);

    //RCCE_barrier(&RCCE_COMM_WORLD);
    //printf(".");
    //RCCE_finalize();

    //taskexitall(0);

}

void run(void *data) {

    ps_init();

    taskexit(0);
}