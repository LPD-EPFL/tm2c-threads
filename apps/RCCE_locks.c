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

int main(int argc, char** argv) {
    RCCE_init(&argc, &argv);
    ID = RCCE_ue();

    P("RCCE_test_lock : %d", RCCE_test_lock(ID));
    P("RCCE_acquire_lock");
    RCCE_acquire_lock(ID);
    P("RCCE_test_lock : %d", RCCE_test_lock(ID));
    P("RCCE_release_lock");
    RCCE_release_lock(ID);
    P("RCCE_test_lock : %d", RCCE_test_lock(ID));
    RCCE_set_lock(ID);
    P("RCCE_test_lock : %d", RCCE_test_lock(ID));
    RCCE_set_lock(ID);
    P("RCCE_test_lock : %d", RCCE_test_lock(ID));


    RCCE_finalize();
    return (EXIT_SUCCESS);
}

