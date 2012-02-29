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

            short j = 0;
    long long int ll = 10000000;

    while (ll--) {
        PF_START(j);
        PF_STOP(j);
    }

    j++;
    ll = 1000000;
    while (ll--) {
        PF_START(j);
        PF_STOP(j);
    }

    j++;
    ll = 100000;
    while (ll--) {
        PF_START(j);
        PF_STOP(j);
    }

    j++;
    ll = 10000;
    while (ll--) {
        PF_START(j);
        PF_STOP(j);
    }

    j++;
    ll = 1000;
    while (ll--) {
        PF_START(j);
        PF_STOP(j);
    }

    j++;
    ll = 100;
    while (ll--) {
        PF_START(j);
        PF_STOP(j);
    }


    j++;
    ll = 10;
    while (ll--) {
        PF_START(j);
        PF_STOP(j);
    }


    j++;
    ll = 1;
    while (ll--) {
        PF_START(j);
        PF_STOP(j);
    }


    PF_PRINT


    TM_END
    EXIT(0);
}
