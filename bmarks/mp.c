/*
 * Testing the message passing latencies
 */

#include <assert.h>

#include "tm.h"


#ifndef SSMP
typedef long long int ticks;
#endif 

#ifdef PLATFORM_TILERA
#include <arch/cycle.h>
#include <tmc/cpus.h>
#define getticks get_cycle_count
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
#ifndef SSMP
typedef long long int ticks;
#endif

MAIN(int argc, char **argv) {

  long long int steps = REPS;

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

#if defined(PLATFORM_TILERA)
    PRINT("running on %d", tmc_cpus_get_my_cpu());
#endif
   BARRIER

      long long int rounds, sum = 0;

    double _start = wtime();
    ticks _start_ticks = getticks();

    int to = (ID - 1)/2;
    PRINT("sending to %d", to);
    //    steps += ID;
    for (rounds = 0; rounds < steps; rounds++) {
#ifdef PGAS
      sum += (int) NONTX_LOAD(sm + rounds);
#else
      //      DUMMY_MSG(rounds % NUM_DSL_NODES);
      DUMMY_MSG(to);
#endif
    }
    //    steps -= ID;
     

    BARRIER;

    ticks _end_ticks = getticks();
    double _end = wtime();

    double _time = _end - _start;
    ticks _ticks = _end_ticks - _start_ticks;
    ticks _ticksm =(ticks) ((double)_ticks / steps);
    double lat = (double) (1000*1000*1000*_time) / steps;
    printf("sent %lld msgs\n\t"
	   "in %f secs\n\t"
	   "%.2f msgs/us\n\t"
	   "%f ns latency\n"
	   "in ticks:\n\t"
	   "in %lld ticks\n\t"
	   "%lld ticks/msg\n", steps, _time, ((double)steps/(1000*1000*_time)), lat,
	   _ticks, _ticksm);


    PRINT("sum -- %lld", sum);

    TM_END
    EXIT(0);
}
