/*
 * A write non-conflicting TX -> a read-only (whole mem) TX -> print results for validation
 */

#include "tm.h"

#define SIS_SIZE 1000

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    TM_INIT

            int *sis = (int *) RCCE_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINTD("RCCE_shmalloc");
        EXIT(-1);
    }

    BARRIER

            int reps = 1;

    FOR(0.3) {
        //        PRINT("@rep %d", reps);

        TX_START

                int i;
        for (i = 0; i < SIS_SIZE; i++) {
            TX_STORE(sis + i, &i, TYPE_INT);
        }

        TX_COMMIT

        TX_START

                int i;
        int s[SIS_SIZE];
        for (i = 0; i < SIS_SIZE; i++) {
            s[i] = *(int *) TX_LOAD(sis + i);
            //printf("%d - ", s[i]);
        }
        //printf("\n");

        //FLUSH

        TX_COMMIT
        reps++;

    }

    TM_END

    EXIT(0);
}