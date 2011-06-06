/*
 * TX with WAR conflicts -> TX with RAW conflicts
 */

#include <assert.h>
#include "tm.h"

#define SIS_SIZE 48

TASKMAIN(int argc, char **argv) {    
    TM_INIT

            int *sis = (int *) RCCE_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINTD("RCCE_shmalloc");
        EXIT(-1);
    }

    shmem_start_address1 = (unsigned int) sis;
    
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
    
    RCCE_shfree((t_vcharp) sis);

    TM_END

    EXITALL(0);
}
