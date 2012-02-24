/*working
 */

#include <stdio.h>

#include "tm.h"

#define SIS_SIZE 480

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    TM_INIT

            int *sis = (int *) sys_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINTD("sys_shmalloc");
        EXIT(-1);
    }

    int i;
    for (i = ID; i < SIS_SIZE; i += 1) {
        *(sis + i) = 1;
    }

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


    ts = RCCE_wtime();

    sum = 0;
    for (i = 0; i < SIS_SIZE; i++) {
        int val = *(int *) (sis + i);
        if (val != 1) {
            PRINTD("\t\t\t\t\t\t*** val %d (shmem : %d) @ +%d", val, *(sis + i), i);
        }
        sum += val;
    }

    te = RCCE_wtime();


    PRINTD("\t\t\t\t\t\t\t\t\t[NO TX] sum: %d in %f secs", sum, te - ts);

    
    BMSTART("NO TX writes")
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
        *(sis + i) = sum;
    }
    BMEND
    
    
    BMSTART("TX writes")
    TX_START
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
        TX_STORE(sis + i, &sum, TYPE_INT);
    }

    //if (ID == 0)
    //  write_set_print(stm_tx->write_set);

    TX_COMMIT
    BMEND


    if (ID == 0) {
        ME;
        for (i = 0; i < SIS_SIZE; i++) {
            printf("%d, ", *(sis + i));
        }
        FLUSH;
    }

    TM_END
    EXIT(0);    
}