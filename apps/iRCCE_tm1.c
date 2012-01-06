/*
 * Benchmarks the time to start and finish an empty TX ->
 * -> the time to start and finish an empty TX that has one (explicitly called) abort
 */

#include "tm.h"
#include "measurements.h"


#define SIS_SIZE 480

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    TM_INIT

    long long int ll = 10000000;
    
    while (ll--) {
        PF_START(0);
        PF_STOP(0);
    }
    
    PF_PRINT
    

    TM_END
    EXIT(0);
}