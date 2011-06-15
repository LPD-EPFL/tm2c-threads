/*
 * Benchmarking the early - release
 */

#include <stdio.h>

#include "tm.h"

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {
    unsigned int SIS_SIZE = 4800;

    TM_INIT

    if (argc > 1) {
        SIS_SIZE = atoi(argv[1]);
    }

    int *sis = (int *) RCCE_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINTD("RCCE_shmalloc");
        EXIT(-1);
    }

    int i;
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
        sis[i] = -1;
    }

    BARRIER

            /*
             * Benchmark SIS_SIZE reads
             */
            double duration = 0;
    double start_;
    TX_START
            int i;
    for (i = 0; i < SIS_SIZE; i++) {
        start_ = RCCE_wtime();
        int j = *(int *) TX_LOAD(sis + i);
        duration += RCCE_wtime() - start_;
    }

    BMSTART("ps_finish_all()")
    ps_finish_all();
    BMEND

    TX_COMMIT

    BARRIER

            /*
             * Benchmark SIS_SIZE reads
             */
    duration = 0;
    TX_START
            int i;
    for (i = 0; i < SIS_SIZE; i++) {
        start_ = RCCE_wtime();
        int j = *(int *) TX_LOAD(sis + i);
        duration += RCCE_wtime() - start_;
    }

    
    int *rl = sis + (100 * sizeof(int));
    char * s1;
    sprintf(s1, "TX_RRLS(%p)", rl);
    BMSTART(s1)
    TX_RRLS(rl)
    BMEND
    rl = sis + (223 * sizeof(int));//
    sprintf(s1, "TX_RRLS(%p)", rl);
    BMSTART(s1)
    TX_RRLS(rl)
    BMEND

    TX_COMMIT

    BARRIER

    RCCE_shfree((t_vcharp) sis);

    TM_END

    EXIT(0);
}
