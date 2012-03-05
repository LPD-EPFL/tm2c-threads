/* 
 * File:   RCCE_locks.c
 * Author: trigonak
 *
 * Created on June 20, 2011, 4:37 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include "iRCCE.h"

unsigned int ID;
#define ME printf("[%02d] ", RCCE_ue())
#define P(args...) ME; printf(args); printf("\n"); fflush(stdout)
#define BMS { double __duration, __stime; __stime = wtime();
#define BME __duration = wtime() - __stime; P("Duration: %f", __duration);}

int main(int argc, char** argv) {
    RCCE_init(&argc, &argv);
    ID = RCCE_ue();

    RCCE_set_lock(ID);

    int i, is_free;
    BMS
    for (i = 0; i < atoi(argv[1]); i++) {
        is_free = RCCE_touch_lock(ID);
    }
    BME
    P("is_lock_free ? %d", is_free);
    
    RCCE_unset_lock(ID);
    P("just used RCCE_unset_lock() : now touch gives: %d", RCCE_touch_lock(ID));
    P("  then touch gives: %d", RCCE_touch_lock(ID));
    
    BMS
    for (i = 0; i < atoi(argv[1]); i++) {
        is_free = RCCE_touch_lock(ID);
    }
    BME
    P("is_lock_free ? %d", is_free);



    RCCE_finalize();
    return (EXIT_SUCCESS);
}

