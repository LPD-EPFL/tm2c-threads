/* 
 * File:   sys_SCC_ssmp.h
 * Author: trigonak
 *
 * Created on May 16 2012, 5:07 PM
 */

#ifndef SYS_SCC_SSMP_H;
#define	SYS_SCC_SSMP_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <ssmp.h>

#include "common.h"
#include "messaging.h"
#include "tm_sys.h"
#include "RCCE.h"
#ifdef PGAS
/*
 * Under PGAS we're using fakemem allocator, to have fake allocations, that
 * mimic those of RCCE_shmalloc
 */
#  include "fakemem.h"
#endif

#define BARRIER  ssmp_barrier_wait(1);
#define BARRIERW ssmp_barrier_wait(0);


  extern tm_addr_t shmem_start_address;
  extern nodeid_t *dsl_nodes;
  extern nodeid_t MY_NODE_ID;
  extern nodeid_t MY_TOTAL_NODES;

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
  extern ssmp_mpb_line_t *abort_reason_mine;
  extern ssmp_mpb_line_t **abort_reasons;
  extern ssmp_mpb_line_t *persisting_mine;
  extern ssmp_mpb_line_t **persisting;
  extern t_vcharp *cm_abort_flags;
  extern t_vcharp cm_abort_flag_mine;
  extern t_vcharp virtual_lockaddress[48];
#endif /* CM_H */


#define PS_BUFFER_SIZE 32
    
EXINLINED nodeid_t
NODE_ID(void)
{
  return MY_NODE_ID;
}

INLINED nodeid_t
TOTAL_NODES(void)
{
  return MY_TOTAL_NODES;
}

INLINED tm_intern_addr_t
to_intern_addr(tm_addr_t addr)
{
#ifdef PGAS
	return fakemem_offset((void*)addr);
#else
	return ((tm_intern_addr_t)((uintptr_t)addr - (uintptr_t)shmem_start_address));
#endif
}


EXINLINED tm_addr_t
to_addr(tm_intern_addr_t i_addr)
{
#ifdef PGAS
	return fakemem_addr_from_offset(i_addr);
#else
	return (tm_addr_t)((uintptr_t)shmem_start_address + i_addr);
#endif
}

INLINED int
sys_sendcmd(void* data, size_t len, nodeid_t to)
{
  //  ssmp_sendl(to, (ssmp_msg_t *) data, len/sizeof(int));
  ssmp_send(to, (ssmp_msg_t *) data);
  return 1;
}

INLINED int
sys_sendcmd_all(void* data, size_t len)
{
  int target;
  for (target = 0; target < NUM_DSL_NODES; target++) {
    //    ssmp_sendl(dsl_nodes[target], (ssmp_msg_t *) data, len/sizeof(int));
    //    PRINT("sending stats to %d", dsl_nodes[target]);
    ssmp_send(dsl_nodes[target], (ssmp_msg_t *) data);
  }
  return 1;
}

INLINED int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
  ssmp_recv_from(from, (ssmp_msg_t *) data, len/sizeof(int));
  return 1;
}

INLINED double
wtime(void)
{
	return RCCE_wtime();
}


#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */

INLINED void
mpb_write_line(ssmp_mpb_line_t *line, uint32_t val)
{
  line->words[0] = val;
  uint32_t w;
  for (w = 1; w < 8; w++)	/* flushing Write Combine Buffer */
    {
      line->words[w] = 0;
    }
}

INLINED void
abort_node(nodeid_t node, CONFLICT_TYPE reason) {
  MPB_INV();
  while (persisting[node]->words[0] == 1)
    {
      MPB_INV();
      wait_cycles(80);
    } 

  uint32_t was_aborted = (*cm_abort_flags[node] == 0);
  if (!was_aborted)
    {
      mpb_write_line(abort_reasons[node], reason);
    }

  MPB_INV();
  while (persisting[node]->words[0] == 1)
    {
      MPB_INV();
      wait_cycles(80);
    } 
      

}

INLINED CONFLICT_TYPE
check_aborted() {
  uint32_t aborted = (*cm_abort_flag_mine == 0);
  *cm_abort_flag_mine = 0;
  return aborted;
}

INLINED CONFLICT_TYPE
get_abort_reason() {
  MPB_INV();
  return abort_reason_mine->words[0];
}

INLINED void
set_tx_running() {
  *cm_abort_flag_mine = 0;
}

INLINED void
set_tx_committed() {
  mpb_write_line(persisting_mine, 0);
}

INLINED CONFLICT_TYPE
set_tx_persisting() {
  mpb_write_line(persisting_mine, 1);
  uint32_t aborted = (*cm_abort_flag_mine == 0);
  if (aborted) {
    mpb_write_line(persisting_mine, 0);
    return 0;
  }

  return 1;
}
#endif	/* NOCM */

#ifdef	__cplusplus
}
#endif

#endif	/* SYS_SCC_SSMP_H */

