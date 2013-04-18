/*
 * Testing the message passing latencies
 */

#include <assert.h>
#include "tm.h"

#ifndef DO_TIMINGS
#  warning **** mp needs DO_TIMINGS defined
#endif

#ifdef PLATFORM_TILERA
#  include <arch/cycle.h>
#  include <tmc/cpus.h>
#  define getticks get_cycle_count
#elif defined(PLATFORM_iRCCE)
typedef long long int ticks;
  EXINLINED ticks getticks(void) {
    ticks ret;

    __asm__ __volatile__("rdtsc" : "=A" (ret));
    return ret;
  }
#endif

#ifdef PLATFORM_iRCCE

EXINLINED ticks getticks(void) {
  ticks ret;
  
  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}

#endif

#define REPS 1000000
#if defined(PLATFORM_MCORE) && !defined(SSMP)
inline ticks getticks(void)
  {
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
  }
#endif


MAIN(int argc, char **argv) 
{

  PF_MSG(0, "roundtrip message");

  long long int steps = REPS;

  PF_MSG(0, "round-trip messaging latencies");

  TM_INIT;

#if !defined(NOCM) && !defined(BACKOFF_RETRY)
  ONCE
    {
      PRINT("*** better run mp with either NOCM and BACKOFF_RETRY to minimize the overhead!\n");
    }
#endif

  if (argc > 1) 
    {
      steps = atoll(argv[1]);
    }


#ifdef PGAS
  int *sm = (int *) sys_shmalloc(steps * sizeof (int));
#endif

  ONCE 
    {
      PRINT("## sending %lld messages", steps);
    }

  BARRIER;

  long long int rounds, sum = 0;

  ticks __start_ticks = getticks();
  nodeid_t to = 0;
  /* PRINT("sending to %d", to);  */

  PF_MSG(3, "Overall time");


  PF_START(3);
  for (rounds = 0; rounds < steps; rounds++) 
    {
      /* PF_START(0); */

#ifdef PGAS
      sum += (int) NONTX_LOAD(sm + rounds);
#else
      DUMMY_MSG(to);
#endif

      /* PF_STOP(0); */

      to++;
      if (to == NUM_DSL_NODES)
      	{
      	  to = 0;
      	}
      /* to %= NUM_DSL_NODES; */
    }
  PF_STOP(3);
  ticks __end_ticks = getticks();
  ticks __duration_ticks = __end_ticks - __start_ticks;
  ticks __ticks_per_sec = (ticks) (1e9 * REF_SPEED_GHZ);
  duration__ = (double) __duration_ticks / __ticks_per_sec;

  if (sum > 0)
    {
      PRINT("sum -- %lld", sum);
    }

  total_samples[3] = steps;
  stm_tx_node->tx_commited = steps;

  TM_END;

  BARRIER;

  /* uint32_t c; */
  /* for (c = 0; c < TOTAL_NODES(); c++) */
  /*   { */
  /* 	if (NODE_ID() == c) */
  /* 	  { */
  /* 	    printf("(( %02d ))", c); */
  /* 	    PF_PRINT; */
  /* 	  } */
  /* 	BARRIER; */
  /*   } */

  EXIT(0);
}
