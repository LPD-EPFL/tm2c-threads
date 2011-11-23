#define INLINED
#include "measurements.h"

#ifdef DO_TIMINGS
ticks entry_time[ENTRY_TIMES_SIZE];
enum timings_bool_t entry_time_valid[ENTRY_TIMES_SIZE] = {M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE,
    M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE};
ticks total_sum_ticks[ENTRY_TIMES_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
long long total_samples[ENTRY_TIMES_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char *measurement_msgs[ENTRY_TIMES_SIZE];
#endif