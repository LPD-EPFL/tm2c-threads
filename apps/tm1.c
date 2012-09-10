/*
 * Benchmarks the time to start and finish an empty TX ->
 * -> the time to start and finish an empty TX that has one (explicitly called) abort
 */

#include "tm.h"

#define SIS_SIZE 480


MAIN(int argc, char **argv) {


    TM_INIT

  int *shmall = (int *) sys_shmalloc(4 * sizeof(int));
    int *shm = shmall + NODE_ID();
  uint32_t res[] = {0, 0};
  uint32_t r;


            short j = 0;
    long long int ll = 10000000;

    while (ll--) {
        PF_START(j);
	r = TX_CAS(shm, ll%2, 1);
        PF_STOP(j);
	res[r]++;
    }


    /* j++; */
    /* ll = 1000000; */
    /* while (ll--) { */
    /*     PF_START(j); */
    /* 	r = TX_CAS(shm, 0, 1); */
    /*     PF_STOP(j); */
    /* 	res[r]++; */
    /* } */

    PRINT("res[f] = %u, res[t] = %u", res[0], res[1]);

    /* j++; */
    /* ll = 100000; */
    /* while (ll--) { */
    /*     PF_START(j); */
    /*     PF_STOP(j); */
    /* } */

    /* j++; */
    /* ll = 10000; */
    /* while (ll--) { */
    /*     PF_START(j); */
    /*     PF_STOP(j); */
    /* } */

    /* j++; */
    /* ll = 1000; */
    /* while (ll--) { */
    /*     PF_START(j); */
    /*     PF_STOP(j); */
    /* } */

    /* j++; */
    /* ll = 100; */
    /* while (ll--) { */
    /*     PF_START(j); */
    /*     PF_STOP(j); */
    /* } */


    /* j++; */
    /* ll = 10; */
    /* while (ll--) { */
    /*     PF_START(j); */
    /*     PF_STOP(j); */
    /* } */


    /* j++; */
    /* ll = 1; */
    /* while (ll--) { */
    /*     PF_START(j); */
    /*     PF_STOP(j); */
    /* } */


    PF_PRINT


    TM_END
    EXIT(0);
}
