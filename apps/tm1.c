/*
 * Benchmarks the time to start and finish an empty TX ->
 * -> the time to start and finish an empty TX that has one (explicitly called) abort
 */

#include "tm.h"

#define SIS_SIZE 480


MAIN(int argc, char **argv) {


    TM_INIT

    uint32_t *shmall = (uint32_t *) sys_shmalloc(2 * TOTAL_NODES() * sizeof(uint32_t));

    uint32_t i = 0;
    while (((((intptr_t) (shmall + i)) >> 3) % NUM_DSL_NODES) > 0) i++;
    uint32_t *shm = shmall + i;

    uint32_t c = 0;
    i = 0;
    while (i++ < NODE_ID()) 
      {
	if (is_app_core(i))
	  {
	    c++;
	  }
      }
    c--;

    BARRIER;
    //    shm += 2*c;

  uint32_t res[] = {0, 0};
  uint32_t r;


  ONCE {
      uint32_t w;
      for (w = 0; w < TOTAL_NODES(); w++)
	{
	  shm[w] = 0;
	}
  }

  BARRIER;


            short j = 0;
	    long long int ll = 100000;

	    uint32_t cnt = 0;

    while (ll--) {
      uint32_t w;
      for (w = 0; w < 16; w++)
	{
	  PF_START(j);
	  r = TX_CASI(shm + w, cnt, cnt+1);
          /* r = TX_CAS(shm, ll%2, 1); */
	  /* r = __sync_bool_compare_and_swap(shm + w, cnt, cnt+1); */
	  PF_STOP(j);
	  res[r]++;
	}
      cnt++;
    }

    BARRIER;

    ONCE {
      uint32_t w;
      for (w = 0; w < TOTAL_NODES(); w++)
	{
	  printf("w# %02d, val %u\n", w, shm[w]);
	}
    }
    FLUSH;
    BARRIER;
    PRINT("res[f] = %u, res[t] = %u", res[0], res[1]);
    BARRIER;

    /* j++; */
    /* ll = 1000000; */
    /* while (ll--) { */
    /*     PF_START(j); */
    /* 	r = TX_CAS(shm, 0, 1); */
    /*     PF_STOP(j); */
    /* 	res[r]++; */
    /* } */



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
