/*
 * Testing PGAS
 */

#include <assert.h>

#include "tm.h"

#define SIS_SIZE 480
#define REPS 31

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    int steps = REPS;


    TM_INIT

    if (argc >= 1) {
        steps = atoi(argv[1]);
    }

    /*
        int once = 1;
        TX_START
        PRINT("will i abort? %d", once);
        TX_LOAD_STORE(1, +, 1);
        if (once--) {
            TX_ABORT(NO_CONFLICT);
        }
        TX_COMMIT
     */

    ONCE
    {
        TX_START
                int i;
        for (i = 0; i < steps; i++) {
            TX_LOAD_STORE(i, +, 1);
        }

        TX_COMMIT
    }
    BARRIER;

    ONCE
    {
        TX_START
                int i, k = (int) ((double) steps / 10) + 1;
        for (i = 0; i < steps; i += k) {
            PRINT("address %2d, value %2d", i, TX_LOAD(i));
        }

        TX_COMMIT
    }
    TM_END
    EXIT(0);
}