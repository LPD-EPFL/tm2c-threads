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
    BMS
    for (i = 0; i < atoi(argv[1]); i++) {
        RCCE_is_lock_free(ID);
    }
    BME



    RCCE_finalize();
    return (EXIT_SUCCESS);
}

