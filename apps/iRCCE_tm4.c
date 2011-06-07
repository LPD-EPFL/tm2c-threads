/*
 * TX with WAR conflicts -> TX with RAW conflicts
 */

#include <assert.h>

#include "tm.h"

#define SIS_SIZE 480

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

    BMSTART("write after read");
    TX_START

            int i;
    for (i = 0; i < SIS_SIZE; i++) {
        if (ID == 1) {
            TX_LOAD(sis + i);
        } else {
            TX_STORE(sis + i, &ID, TYPE_INT);
        }
    }

    TX_COMMIT
    BMEND
    
    BARRIER
    

    BMSTART("read after write");
    TX_START

    if(ID == 1) {
        PRINTD("sleeping");
        udelay(3500000);
    }
            int i;
    for (i = 0; i < SIS_SIZE; i++) {
        if (ID == 1) {
            TX_LOAD(sis + i);
        } else {
            TX_STORE(sis + i, &ID, TYPE_INT);
        }
    }

    TX_COMMIT
    BMEND
    
    RCCE_shfree((t_vcharp) sis);

    TM_END

    EXIT(0);
}
