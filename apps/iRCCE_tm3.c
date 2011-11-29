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
    
    FOR(1) {
        PGAS_alloc();
    }
    
    PRINT("%d", PGAS_alloc());
    
    TX_COMMIT

    TM_END
    EXIT(0);
}