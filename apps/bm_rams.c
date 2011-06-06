#include <assert.h>

#include "tm.h"

#define S 1000

#define TEST2

MAIN(int argc, char **argv) {

    RCCE_init(&argc, &argv);
    int ID = RCCE_ue();
    int NUM_UES = RCCE_num_ues();
    
    int *sis = (int *) RCCE_shmalloc(S * sizeof(int));
    int *pis = (int *) malloc(S * sizeof(int));
    if (sis == NULL || pis == NULL) {
        PRINTD("sis || pis == NULL");
        EXIT(-1);
    }
    
#ifdef TEST1
    
    PRINTD("BM %d WRITEs -> READs ----------------------", S);
    
    BMSTART("using PRAM")
    int i;
    for (i = 0; i < S; i++) {
        pis[i] = i;
    }
    int temp;
    for (i = 0; i < S; i++) {
        temp = pis[i];
        temp++;
    }
    BMEND
    
    BMSTART("using SRAM")
    int i;
    for (i = 0; i < S; i++) {
        sis[i] = i;
    }
    int temp;
    for (i = 0; i < S; i++) {
        temp = sis[i];
        temp++;
    }
    BMEND
    
#elif defined(TEST2)
    
    PRINTD("BM %d READs -> WRITEs -> READs ----------------------", S);
    BMSTART("using PRAM")
    int i;
    int temp;
    for (i = 0; i < S; i++) {
        temp = pis[i];
        temp++;
    }
    for (i = 0; i < S; i++) {
        pis[i] = i;
    }
    for (i = 0; i < S; i++) {
        temp = pis[i];
        temp++;
    }
    BMEND
    BMSTART("using SRAM")
    int i;
    int temp;
    for (i = 0; i < S; i++) {
        temp = sis[i];
        temp++;
    }
    for (i = 0; i < S; i++) {
        sis[i] = i;
    }
    for (i = 0; i < S; i++) {
        temp = sis[i];
        temp++;
    }
    BMEND
    
#endif
    
    EXIT(0);
}