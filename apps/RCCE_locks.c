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
#define BMS { double __duration, __stime; __stime = RCCE_wtime();
#define BME __duration = RCCE_wtime() - __stime; P("Duration: %f", __duration);}

int main(int argc, char** argv) {
    RCCE_init(&argc, &argv);
    ID = RCCE_ue();

    P("starting");

    int i;
    for (i = 0; i < RCCE_num_ues(); i++) {
        int is_free;
        P("is_free %02d", i);
        BMS
        is_free = RCCE_is_lock_free(i);
        BME
        P("set %02d", i);
        BMS
        RCCE_set_lock(i);
        BME
        P("is_free %02d", i);
        BMS
        is_free = RCCE_is_lock_free(i);
        BME
        P("rls %02d", i);
        BMS
        is_free = RCCE_release_lock(i);
        BME
    }



    RCCE_finalize();
    return (EXIT_SUCCESS);
}

