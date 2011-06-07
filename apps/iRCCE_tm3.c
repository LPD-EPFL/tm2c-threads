/*
 * Test if the write-set -> read-set work properly
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

    //testing write set
    TX_START
    TX_STORE(sis + ID, &ID, TYPE_INT);
    ME; printf("sis[%d] = %d | ", ID, *(int *) TX_LOAD(&sis[ID]));
    int doubleID = 2 * ID;
    TX_STORE(sis + ID, &doubleID, TYPE_INT);
    printf("sis[%d] = %d\n", ID, *(int *) TX_LOAD(&sis[ID]));
    FLUSH
    TX_COMMIT

    //testing read set
    TX_START
    sis[ID] == 665;
    int tripleID = 3 * ID;
    TX_STORE(sis + ID, &tripleID, TYPE_INT);
    int temp = *(int *) TX_LOAD(&sis[ID]);
    ME; printf("sis[%d] = %d | ", ID, temp);
    assert(temp == 3 * ID);
    sis[ID] = 666; //cheating TM
    temp = *(int *) TX_LOAD(&sis[ID]);
    printf("sis[%d] = %d\n", ID, temp);
    FLUSH
    TX_COMMIT
    
    PRINTD("finally : sis[%d] = %d\n", ID, sis[ID]);

    RCCE_shfree((t_vcharp) sis);
    
    TM_END
    EXIT(0);
}