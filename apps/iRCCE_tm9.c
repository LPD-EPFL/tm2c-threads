/*
 * TX with WAR conflicts -> TX with RAW conflicts
 */

#include "tm.h"

#define SIS_SIZE 48

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;


MAIN(int argc, char **argv) {    
    TM_INIT
    
    int * i = (int *) malloc(10);
    free(i);
    
    int *j = (int *) RCCE_shmalloc(10);
    RCCE_shfree(j);

    BARRIER
    
    /*
     * Write after read conflicts : some writers and some readers on the whole
     * memory
     */
    
    TX_START
    
    int * i = (int *) malloc(10);
    free(i);
    
    int *j = (int *) RCCE_shmalloc(10);
    RCCE_shfree(j);

    TX_COMMIT
    
    BARRIER

    TM_END

    EXITALL(0);
}
