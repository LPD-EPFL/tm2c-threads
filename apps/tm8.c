/*
 * TX with WAR conflicts -> TX with RAW conflicts
 */

#include <assert.h>
#include "tm.h"

#define SIS_SIZE 48


stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {    
    TM_INIT

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
     * Write after read conflicts : some writers and some readers on the whole
     * memory
     */
    
    TX_START
    
    if (!ID) {
        taskudelay(100);
    }
    
            int i;
    for (i = 2; i < SIS_SIZE; i++) {
        if (ID >= 1) {
            TX_LOAD(sis + i);
        } else {
            TX_STORE(sis + i, &ID, TYPE_INT);
        }
    }

    TX_COMMIT
    
    BARRIER
    
    /*
     * Write after read conflicts : some writers and some readers on the whole
     * memory
     */
    
    TX_START
    
    if (!ID) {
        taskudelay(100);
    }
    
            int i;
    for (i = 0; i < SIS_SIZE; i++) {
        if (ID >= 1) {
            TX_LOAD(sis + i);
        } else {
            TX_STORE(sis + i, &ID, TYPE_INT);
        }
    }

    TX_COMMIT
    
    BARRIER
    
    sys_shfree((sys_t_vcharp) sis);

    TM_END

    EXITALL(0);
}
