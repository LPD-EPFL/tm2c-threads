/*
 * Testing PGAS
 */

#include <assert.h>

#include "tm.h"

#define SIS_SIZE 480

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    int steps = 0;

    TM_INIT


    TX_START
    
    TX_LOAD_STORE(0, +, 1);
    TX_LOAD_STORE(1, +, 1);
    TX_LOAD_STORE(2, +, 1);
    
    TX_COMMIT

    TM_END
    EXIT(0);
}