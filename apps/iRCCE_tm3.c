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
    int i;
    for (i = 0; i < 10; i++) {
        TX_LOAD_STORE(i, +, i);
    }
    
    TX_COMMIT
    TX_START
    int i;
    for (i = 0; i < 10; i++) {
            PRINT("address %2d, value %2d", i, TX_LOAD(i));
    }
    
    TX_COMMIT

    TM_END
    EXIT(0);
}