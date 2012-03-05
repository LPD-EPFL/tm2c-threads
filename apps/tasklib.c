/*working
 */

#include "pubSubTM.h"

#include "task.h"

void run(void *);

MAIN(int argc, char **argv) {

    RCCE_init(&argc, &argv);

    //taskcreate(run, (void *) ID, 20480);


    ps_init_();


    int ID = RCCE_ue();

    int wait_time = atoi(argv[1]) + 300 * (ID + 1);

    double totaltime = 0;
    long long int i = 1;


    void *shmp = (void *) sys_shmalloc(1);
    if (shmp == NULL) {
        PRINTD("got null");
        taskexit(-1);
    }


    taskdelay(10);

    int lim = atoi(argv[2]);

    while (i++ < lim) {
        taskudelay(wait_time);

        shmp += ID;
        double start = wtime();
        //ps_subscribe(shmp);
        ps_publish(shmp);
        double diff = wtime() - start;
        totaltime += diff;
        //PRINTD("\t\t\t\t %lld|completed for %d in %f", (i - 1), target, diff);

    }

    PRINTD("\t\t\t\t\t\t\t>>completed in avg: %f sec.", totaltime / (i - 1));
  

    taskdelay(10000);
    taskexit(0);

    
    //printf(".");
    //RCCE_finalize();

    //taskexitall(0);

}

void run(void *data) {

    ps_init();

    taskexit(0);
}
