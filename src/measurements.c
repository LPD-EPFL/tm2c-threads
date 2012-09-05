#define EXINLINED
#include "measurements.h"

#ifdef DO_TIMINGS
ticks entry_time[ENTRY_TIMES_SIZE];
enum timings_bool_t entry_time_valid[ENTRY_TIMES_SIZE] = {M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE};
ticks total_sum_ticks[ENTRY_TIMES_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
long long total_samples[ENTRY_TIMES_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char *measurement_msgs[ENTRY_TIMES_SIZE];
ticks getticks_correction = 0;

ticks getticks_correction_calc() {
#define GETTICKS_CALC_REPS 1000000
  ticks t_dur = 0;
  uint32_t i;
  for (i = 0; i < GETTICKS_CALC_REPS; i++) {
    ticks t_start = getticks();
    ticks t_end = getticks();
    t_dur += t_end - t_start;
  }
  printf("corr in float %f\n", (t_dur / (double) GETTICKS_CALC_REPS));
  getticks_correction = (ticks)(t_dur / (double) GETTICKS_CALC_REPS);
  return getticks_correction;
}
#endif
