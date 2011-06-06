/*working
 */

#include <stdio.h>

#include "tm.h"

#define SIS_SIZE 480

TASKMAIN(int argc, char **argv) {

    TM_INIT

            int *sis = (int *) RCCE_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINTD("RCCE_shmalloc");
        EXIT(-1);
    }

    PRINTD("~ %p", sis);

    int i;
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
        *(sis + i) = 1;
    }

    BARRIER

            int sum;

    double ts = RCCE_wtime();
    TX_START

    sum = 0;
    for (i = 0; i < SIS_SIZE; i++) {
        int val = *(int *) TX_LOAD(sis + i);
        if (val != 1) {
            PRINTD("\t\t\t\t\t\t*** val %d (shmem : %d) @ +%d", val, *(sis + i), i);
        }
        sum += val;
    }

    TX_COMMIT
            double te = RCCE_wtime();


    PRINTD("\t\t\t\t\t\t\t\t\t[TX] sum: %d in %f secs", sum, te - ts);

    BARRIER

    ts = RCCE_wtime();

    sum = 0;
    for (i = 0; i < SIS_SIZE; i++) {
        taskyield();
        int val = *(int *) (sis + i);
        if (val != 1) {
            PRINTD("\t\t\t\t\t\t*** val %d (shmem : %d) @ +%d", val, *(sis + i), i);
        }
        sum += val;
    }

    te = RCCE_wtime();


    PRINTD("\t\t\t\t\t\t\t\t\t[NO TX] sum: %d in %f secs", sum, te - ts);

    
    BARRIER
    
    BMSTART("NO TX writes")
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
        taskyield();
        *(sis + i) = sum;
    }
    BMEND
    
    BARRIER
    
    BMSTART("TX writes")
    TX_START
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
        TX_STORE(sis + i, &sum, TYPE_INT);
    }

    //if (ID == 0)
    //  write_set_print(stm_tx->write_set);

    TX_COMMIT
    BMEND

    BARRIER

    if (ID == 0) {
        ME;
        for (i = 0; i < SIS_SIZE; i++) {
            printf("%d, ", *(sis + i));
        }
        FLUSH;
    }

    TM_END
    taskexit(0);
}