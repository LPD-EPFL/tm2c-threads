/*
 * Testing the message passing latencies
 */

#include <assert.h>
#include "tm.h"

#ifndef DO_TIMINGS
#  error mp needs DO_TIMINGS defined
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


MAIN(int argc, char **argv) {

  PF_MSG(0, "roundtrip message");

  long long int steps = REPS;

  PF_MSG(0, "round-trip messaging latencies");

  TM_INIT

    if (argc > 1) {
      steps = atoll(argv[1]);
    }


#ifdef PGAS
  int *sm = (int *) sys_shmalloc(steps * sizeof (int));
#endif

  ONCE {
    PRINT("## sending %lld messages", steps);
  }

  BARRIER

    long long int rounds, sum = 0;

  double _start = wtime();
  ticks _start_ticks = getticks();

  nodeid_t to = 0;
  /* PRINT("sending to %d", to);  */

  PF_MSG(3, "Overall time");


  PF_START(3);
  for (rounds = 0; rounds < steps; rounds++) {
    PF_START(0);
#ifdef PGAS
    sum += (int) NONTX_LOAD(sm + rounds);
#else
    DUMMY_MSG(to);
#endif
    PF_STOP(0);

    if (++to == NUM_DSL_NODES)
      {
	to = 0;
      }
  }
  PF_STOP(3);

  if (sum > 0)
    {
      PRINT("sum -- %lld", sum);
    }

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
