#ifndef DSLOCK_H
#define DSLOCK_H

#include "common.h"
#include "pubSubTM.h"
#include "ps_hashtable.h"

#ifdef PGAS
#include "pgas.h"
extern write_set_pgas_t **PGAS_write_sets;
#endif

void dsl_init(void);

void dsl_communication();

inline CONFLICT_TYPE try_subscribe(nodeid_t nodeId, tm_addr_t shmem_address);
inline CONFLICT_TYPE try_publish(nodeid_t nodeId, tm_addr_t shmem_address);
inline void unsubscribe(nodeid_t nodeId, tm_addr_t shmem_address);
inline void publish_finish(nodeid_t nodeId, tm_addr_t shmem_address);
inline void print_global_stats();
inline void print_hashtable_usage();

extern unsigned long int stats_total, stats_commits, stats_aborts, stats_max_retries, stats_aborts_war,
        stats_aborts_raw, stats_aborts_waw, stats_received;
extern double stats_duration;

extern ps_hashtable_t ps_hashtable;
extern unsigned int NUM_UES_APP;

extern unsigned int ID; //=RCCE_ue()
extern unsigned int NUM_UES;

#endif
