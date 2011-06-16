/*
 * Benchmarks the time to start and finish an empty TX ->
 * -> the time to start and finish an empty TX that has one (explicitly called) abort
 */

#include "tm.h"

#define SIS_SIZE 480

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

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
    
    BMSTART("time to start and end an empty TX");
    TX_START
    TX_COMMIT
    BMEND

    BMSTART("time to start and end an empty TX once aborted TX");
    int aborted = 0;
    TX_START
    if (!aborted) {
        aborted = 1;
        TX_ABORT(1)
    }
    TX_COMMIT
    BMEND





    TM_END
    EXIT(0);
}