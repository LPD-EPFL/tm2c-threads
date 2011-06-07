/*
 * A write non-conflicting TX -> a read-only (whole mem) TX -> print results for validation
 */

#include "tm.h"

#define SIS_SIZE 4800

MAIN(int argc, char **argv) {

    TM_INIT

            int *sis = (int *) RCCE_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINTD("RCCE_shmalloc");
        EXIT(-1);
    }

    BARRIER

    BMSTART("write");
    TX_START        
    int i;
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
        TX_STORE(sis + i, &ID, TYPE_INT);
    }
    TX_COMMIT
    BMEND
    
    BARRIER
    
    int lis[SIS_SIZE];
    BMSTART("read")
    TX_START
    int i;
    for (i = 0; i < SIS_SIZE; i++) {
        lis[i] = *(int *) TX_LOAD(sis + i);
    }
    TX_COMMIT
    BMEND
    
    BARRIER
    
    if (ID == 0) {
        int i;
        for (i = 0; i < SIS_SIZE; i++) {
            printf("%d, ", lis[i]);
        }
        printf("\n");
        FLUSH;
    }
    

    TM_END
    EXIT(0);
}