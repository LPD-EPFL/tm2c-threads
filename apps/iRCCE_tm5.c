/*
 * TX with WAW conflicts
 */

#include <assert.h>

#include "tm.h"

#define SIS_SIZE 20


stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    TM_INIT

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
     * Write after write conflicts : everyone writes the whole mem
     * (starting from ID, not 0 ~ almost the whole mem)
     */
    TX_START
    if (ID == 0) {
        taskudelay(500);
    }
    int i;
    for (i = ID; i < SIS_SIZE; i++) {
        TX_STORE(sis + i, &ID, TYPE_INT);
    }
    TX_COMMIT
    

    PRINTD("  | Entering barrier");
    BARRIER

    RCCE_shfree((t_vcharp) sis);

    TM_END
    //
    EXITALL(0);
}
