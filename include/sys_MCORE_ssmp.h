#ifndef _SYS_MCORE_H_
#define _SYS_MCORE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <ssmp.h>

#include "common.h"
#include "messaging.h"
#include "mcore_malloc.h"
#include "fakemem.h"


#define BARRIER  ssmp_barrier_wait(1);
#define BARRIERW ssmp_barrier_wait(0);
#define BARRIER_DSL ssmp_barrier_wait(14);

extern nodeid_t MY_NODE_ID;
extern nodeid_t MY_TOTAL_NODES;

extern PS_REPLY* ps_remote_msg; // holds the received msg
extern nodeid_t *dsl_nodes;

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
extern int32_t **cm_abort_flags;
extern int32_t *cm_abort_flag_mine;
#endif /* CM_H */

/* --- inlined methods --- */
INLINED nodeid_t
NODE_ID(void)
{
  return MY_NODE_ID;
}


extern int is_app_core(int id);

INLINED nodeid_t
TOTAL_NODES(void)
{
  return MY_TOTAL_NODES;
}

INLINED int
sys_sendcmd(void* data, size_t len, nodeid_t to)
{
  /* PF_START(10); */
  ssmp_send(to, (ssmp_msg_t *) data);
  /* PF_STOP(10); */
}

INLINED int
sys_sendcmd_all(void* data, size_t len)
{
  int target;
  for (target = 0; target < NUM_DSL_NODES; target++) {
    ssmp_send(dsl_nodes[target], (ssmp_msg_t *) data);
  }
  return 1;
}

static const uint32_t wait_after_send[4] = {825, 830, 830, 840};
/* #define vv 800 */
/* static const uint32_t wait_after_send[4] = {vv, vv, vv, vv}; */

INLINED int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
  uint32_t hops = get_num_hops(NODE_ID(), from);
  wait_cycles(wait_after_send[hops]);

  /* PF_START(9); */
  ssmp_recv_from(from, (ssmp_msg_t *) data);
  /* PF_STOP(9); */
}

INLINED tm_intern_addr_t
to_intern_addr(tm_addr_t addr)
{
#ifdef PGAS
  return fakemem_offset((void*) addr);
#else
  return (tm_intern_addr_t)addr;
#endif
}

INLINED tm_addr_t
to_addr(tm_intern_addr_t i_addr)
{
#ifdef PGAS
#else
  return (tm_addr_t)i_addr;
#endif
}

INLINED double
wtime(void)
{
  struct timeval t;
  gettimeofday(&t,NULL);
  return (double)t.tv_sec + ((double)t.tv_usec)/1000000.0;
}


#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
INLINED void
abort_node(nodeid_t node, CONFLICT_TYPE reason) {
  //  uint32_t p = 0;
  while (__sync_val_compare_and_swap(cm_abort_flags[node], NO_CONFLICT, reason) == PERSISTING_WRITES) {
    //    p++; 
    wait_cycles(180); 
  }

  /* if (p > 0) { */
  /*   PRINT("Wait %3d rounds", p); */
  /* } */

}

INLINED CONFLICT_TYPE
check_aborted() {
  return (*cm_abort_flag_mine != NO_CONFLICT);
}

INLINED CONFLICT_TYPE
get_abort_reason() {
  return (*cm_abort_flag_mine);	
}

INLINED void
set_tx_running() {
  *cm_abort_flag_mine = NO_CONFLICT;
}

INLINED void
set_tx_committed() {
  *cm_abort_flag_mine = TX_COMMITED;
}

INLINED CONFLICT_TYPE
set_tx_persisting() {
  return __sync_bool_compare_and_swap(cm_abort_flag_mine, NO_CONFLICT, PERSISTING_WRITES);
}
#endif	/* NOCM */

#endif
