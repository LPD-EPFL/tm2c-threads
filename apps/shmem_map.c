#include "common.h"

#define MEMSIZE 480

MAIN(int argc, char **argv) {

    RCCE_init(&argc, &argv);

    unsigned short int ID = RCCE_ue();
    unsigned short int NUM_UES = RCCE_num_ues();
    
    int *is = (int *) RCCE_shmalloc(MEMSIZE * sizeof(int));
    if (is == NULL) {
        PRINTD("alloc xx");
        EXIT(-1);
    }
    
    if (ID == 0) {
        PRINTD("starting shmem addr: %p", is);
    }
    
    int i;
    for (i = ID; i < MEMSIZE; i += NUM_UES) {
        is[i] = 1;
    }
    
    while(1) {
        taskdelay(1000);
    }


    RCCE_finalize();
    taskexit(0);
}
