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

INLINED CONFLICT_TYPE try_subscribe(nodeid_t nodeId, tm_addr_t shmem_address);
INLINED CONFLICT_TYPE try_publish(nodeid_t nodeId, tm_addr_t shmem_address);

void print_global_stats();
void print_hashtable_usage();

extern unsigned long int stats_total, stats_commits, stats_aborts, stats_max_retries, stats_aborts_war,
        stats_aborts_raw, stats_aborts_waw, stats_received;
extern double stats_duration;

extern ps_hashtable_t ps_hashtable;

INLINED CONFLICT_TYPE try_subscribe(nodeid_t nodeId, tm_addr_t shmem_address) {

    return ps_hashtable_insert(ps_hashtable, nodeId, (uintptr_t)shmem_address, READ);;
}

INLINED CONFLICT_TYPE try_publish(nodeid_t nodeId, tm_addr_t shmem_address) {

    return ps_hashtable_insert(ps_hashtable, nodeId, (uintptr_t)shmem_address, WRITE);;
}

INLINED void unsubscribe(nodeid_t nodeId, tm_addr_t shmem_address) {
    ps_hashtable_delete(ps_hashtable, nodeId, (uintptr_t)shmem_address, READ);
}

INLINED void publish_finish(nodeid_t nodeId, tm_addr_t shmem_address) {
    ps_hashtable_delete(ps_hashtable, nodeId, (uintptr_t)shmem_address, WRITE);
}

#endif
