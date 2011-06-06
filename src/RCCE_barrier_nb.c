/* 
 * File:   RCCE_barrier_nb.c
 * Author: trigonak
 *
 * Created on April 30, 2011, 5:47 PM
 */

#include "iRCCE_lib.h"
#include "task.h"

//--------------------------------------------------------------------------------------
// FUNCTION: RCCE_barrier_nb
//--------------------------------------------------------------------------------------
// very simple, linear barrier 
//--------------------------------------------------------------------------------------

int RCCE_barrier_nb(RCCE_COMM *comm) {

    int counter, i;
    int ROOT = 0;
    t_vchar cyclechar;
    t_vchar valchar;
    t_vcharp gatherp;
    RCCE_FLAG_STATUS cycle;
    int bit_location = comm->gather.location % RCCE_FLAGS_PER_BYTE;

    counter = 0;
    gatherp = comm->gather.flag_addr;

    if (!comm || !(comm->initialized))
        RCCE_error_return(RCCE_debug_synch, RCCE_ERROR_COMM_UNDEFINED);
    // flip local barrier variable                                      
    RCCE_get_char(&cyclechar, gatherp, RCCE_IAM);
#ifdef SINGLEBITFLAGS
    cycle = RCCE_flip_bit_value(&cyclechar, bit_location);
#else
    cyclechar = (t_vchar) (!((unsigned int) (cyclechar)));
    cycle = (unsigned int) cyclechar;
#endif
    RCCE_put_char(gatherp, &cyclechar, RCCE_IAM);

    if (RCCE_IAM == comm->member[ROOT]) {
        // read "remote" gather flags; once all equal "cycle" (i.e counter==comm->size), 
        // we know all UEs have reached the barrier                   
        while (counter != comm->size) {
            // skip the first member (#0), because that is the ROOT         
            for (counter = i = 1; i < comm->size; i++) {
                // copy flag values out of comm buffer                        
                RCCE_get_char(&valchar, gatherp, comm->member[i]);
#ifdef SINGLEBITFLAGS
                if (RCCE_bit_value(&valchar, bit_location) == cycle) counter++;
#else 
                if (valchar == cyclechar) counter++;
#endif
            }
            //taskyield();
            taskndelay(1);
        }
        // set release flags                                              
        for (i = 1; i < comm->size; i++)
            RCCE_flag_write(&(comm->release), cycle, comm->member[i]);
    } else {
        int result = 0;
        while (!result) {
            iRCCE_test_flag(comm->release, cycle, &result);
            //taskyield();
            taskndelay(1);
        }
        //RCCE_wait_until(comm->release, cycle);
    }
    if (RCCE_debug_synch) fprintf(STDERR, "UE %d has cleared barrier\n", RCCE_IAM);
    return (RCCE_SUCCESS);
}

