/*
 * A write non-conflicting TX -> a read-only (whole mem) TX -> print results for validation
 */

#include "tm.h"

#define SIS_SIZE 100

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    TM_INIT


    BARRIER

            int reps = 1;

FOR(0.3) {
        //        PRINT("@rep %d", reps);

        TX_START

                int i;
        for (i = 0; i < SIS_SIZE; i++) {
            TX_STORE(4 * i, reps * i);
        }

        TX_COMMIT

        TX_START

                int i;
        int s[SIS_SIZE];
        for (i = 0; i < SIS_SIZE; i++) {
            s[i] = TX_LOAD(4 * i);
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