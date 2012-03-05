/*
 * Benchmarking reads in read-only TXs (1 pass: no use of read-buffering) -> 
 * -> read-only TXs (2 passes: the second pass is all from read set
 */

#include "tm.h"

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {
    unsigned int SIS_SIZE = 4800;
    
    TM_INIT
    
     if (argc > 1) {
        SIS_SIZE = atoi(argv[1]);
    }

            int *sis = (int *) sys_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINTD("sys_shmalloc");
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
        start_ = wtime();
        int j = *(int *) TX_LOAD(sis + i);
        duration += wtime() - start_;
    }
    TX_COMMIT
    
    PRINT("Completed %d TX reads in %f secs. Time per READ = %f", SIS_SIZE, duration, duration / SIS_SIZE);

    BARRIER
    
    /*
     * Benchmark 2*SIS_SIZE reads : the second pass exist in the read set (buffered)!
     */
    duration = 0;
    TX_START
    int i;
    for (i = 0; i < SIS_SIZE; i++) {
        start_ = wtime();
        int j = *(int *) TX_LOAD(sis + i);
        duration += wtime() - start_;
    }
    for (i = 0; i < SIS_SIZE; i++) {
        start_ = wtime();
        int j = *(int *) TX_LOAD(sis + i);
        duration += wtime() - start_;
    }
    TX_COMMIT
    
    PRINT("Completed %d TX reads in %f secs. Time per READ = %f", 2 * SIS_SIZE, duration, duration / (2 * SIS_SIZE));

    BARRIER
    
    sys_shfree((t_vcharp) sis);

    TM_END

    EXIT(0);
}
