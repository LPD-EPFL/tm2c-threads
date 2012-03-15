/*
 * TX with WAR conflicts -> TX with RAW conflicts
 */

#include "tm.h"

#define SIS_SIZE 100

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;


MAIN(int argc, char **argv) {    
    TM_INIT
    
    int reps = SIS_SIZE;
    
    if (argc > 1) {
        reps = atoi(argv[1]);
    }
    
    ONCE {
        PRINT("Repetitions %d", reps);
    }
    
    int *j = (int *) sys_shmalloc(reps);

    BARRIER
    
    /*
     * Write after read conflicts : some writers and some readers on the whole
     * memory
     */
    
    TX_START
    
    int i;
    for (i = 0; i < reps; i++) {
        //PRINT("loading %2d, %p", i, j+i);
        TX_LOAD(j + i);
    }

    TX_COMMIT
    
    BARRIER

    TM_END

    EXITALL(0);
}
