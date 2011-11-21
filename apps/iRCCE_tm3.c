/*
 * Testing PGAS
 */

#include <assert.h>

#include "tm.h"

#define SIS_SIZE 480

stm_tx_t *stm_tx;
stm_tx_node_t *stm_tx_node;

MAIN(int argc, char **argv) {

    TM_INIT

    TX_START

            int i;
    for (i = 0; i < 5; i++) {
        PRINT("loading: %2d, value: %d", i, *(int *) TX_LOAD(i));
    }
    TX_COMMIT

    TM_END
    EXIT(0);
}