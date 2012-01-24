/*
 * Testing PGAS
 */

#include <assert.h>

#include "tm.h"

#define SIS_SIZE 480
#define REPS 1000

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    int steps = REPS;


    PF_MSG(0, "TX_LOAD");
    PF_MSG(1, "send & receive");
    PF_MSG(2, "send");
    PF_MSG(3, "receive");


    TM_INIT

    if (argc >= 1) {
        steps = atoi(argv[1]);
    }

    int *sm = (int *) RCCE_shmalloc(steps * sizeof (int));

    BARRIER
    if (argc < 3) { //ONLY 1 cores sending messages
        ONCE
        {
            int sum;

            int rounds = 1;

            PF_START(0)
            while (REPS * rounds++ <= steps) {
                TX_START
                sum = 0;
                int i;
                for (i = 0; i < REPS; i++) {
                    int *addr = sm + i + (rounds * REPS);
                    sum += *(int *) TX_LOAD(addr);
                }
                TX_COMMIT
            }
            PF_STOP(0)
            PRINT("sum -- %d", sum);
            PF_PRINT
        }
    }
    else { //ALL cores sending messages
        int sum;

        int rounds = 1;

        while (REPS * rounds++ <= steps) {
            TX_START
            sum = 0;
            int i;
            for (i = 0; i < REPS; i++) {
                //int *addr = sm + i + (rounds * REPS);

                int *addr = sm + ((i + (rounds * REPS) + ID) % steps);
                PF_START(0)
                sum += *(int *) TX_LOAD(addr);
                PF_STOP(0)
            }

            TX_COMMIT
        }
        PRINT("sum -- %d", sum);
        PF_PRINT
    }


    BARRIER;

    TM_END
    EXIT(0);
}