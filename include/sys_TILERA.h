/* 
 * File:   sys_TILERA.h
 * Author: trigonak
 *
 * Created on April 30, 2012, 5:07 PM
 */

#ifndef SYS_TILERA_H
#define	SYS_TILERA_H

#include "common.h"
#include "messaging.h"
#include <tmc/mem.h>

#ifdef PGAS
/*
 * Under PGAS we're using fakemem allocator, to have fake allocations, that
 * mimic those of RCCE_shmalloc
 */
#include "fakemem.h"
#endif

#ifdef	__cplusplus
extern "C" {
#endif

  extern nodeid_t *dsl_nodes;
#define PS_BUFFER_SIZE 32

#if !defined(NOCM) && !defined(BACKOFF_RETRY) 			/* if any other CM (greedy, wholly, faircm) */
  extern int32_t **cm_abort_flags;
  extern int32_t *cm_abort_flag_mine;
#endif /* CM_H */

  extern DynamicHeader *udn_header; //headers for messaging
  extern uint32_t* demux_tags;
  extern tmc_sync_barrier_t *barrier_apps, *barrier_all, *barrier_dsl; //BARRIERS


  INLINED nodeid_t
  NODE_ID(void) 
  {
    return (nodeid_t) ID;
  }

  INLINED nodeid_t
  TOTAL_NODES(void) 
  {
    return (nodeid_t) NUM_UES;
  }

  INLINED tm_intern_addr_t
  to_intern_addr(tm_addr_t addr) {
#ifdef PGAS
    return fakemem_offset((void*) addr);
#else
    return ((tm_intern_addr_t) (addr));
#endif
  }

  INLINED tm_addr_t
  to_addr(tm_intern_addr_t i_addr) {
#ifdef PGAS
    return fakemem_addr_from_offset(i_addr);
#else
    /* return (tm_addr_t) ((uintptr_t) shmem_start_address + i_addr); */
    return (tm_addr_t) (i_addr);
#endif
  }

  INLINED int
  sys_sendcmd(void* data, size_t len, nodeid_t to)
  {
    tmc_udn_send_buffer(udn_header[to], UDN0_DEMUX_TAG, data, PS_COMMAND_SIZE_WORDS);
    /* tmc_udn_send_buffer(udn_header[to], UDN0_DEMUX_TAG, data, len/sizeof(int_reg_t)); */
    /* tmc_udn_send_buffer(udn_header[to], demux_tags[to], data, len/sizeof(int_reg_t)); */
  }


  INLINED int
  sys_sendcmd_all(void* data, size_t len)
  {
    int target;
    for (target = 0; target < NUM_DSL_NODES; target++)
      {
	tmc_udn_send_buffer(udn_header[dsl_nodes[target]], UDN0_DEMUX_TAG, data, PS_STATS_CMD_SIZE_WORDS);
	/* nodeid_t dsl_id = dsl_nodes[target]; */
	/* tmc_udn_send_buffer(udn_header[dsl_id], demux_tags[dsl_id], data, len/sizeof(int_reg_t)); */
      }
    return 1;
  
  }

  INLINED int
  sys_recvcmd(void* data, size_t len, nodeid_t from)
  {
    tmc_udn0_receive_buffer(data, PS_REPLY_SIZE_WORDS);

    /* switch (demux_tags[from]) */
    /*   { */
    /*   case UDN0_DEMUX_TAG: */
    /* 	tmc_udn0_receive_buffer(data, len/sizeof(int_reg_t)); */
    /* 	break; */
    /*   case UDN1_DEMUX_TAG: */
    /* 	tmc_udn1_receive_buffer(data, len/sizeof(int_reg_t)); */
    /* 	break; */
    /*   case UDN2_DEMUX_TAG: */
    /* 	tmc_udn2_receive_buffer(data, len/sizeof(int_reg_t)); */
    /* 	break; */
    /*   default:			/\* 3 *\/ */
    /* 	tmc_udn3_receive_buffer(data, len/sizeof(int_reg_t)); */
    /* 	break; */
    /*   } */
  }

  INLINED double
  wtime(void) 
  {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (double) t.tv_sec + ((double) t.tv_usec) / 1000000.0;
  }


#if !defined(NOCM) && !defined(BACKOFF_RETRY) /* if any other CM (greedy, wholly, faircm) */
  INLINED void
  abort_node(nodeid_t node, CONFLICT_TYPE reason)
  {
    while(arch_atomic_val_compare_and_exchange(cm_abort_flags[node], NO_CONFLICT, reason) == PERSISTING_WRITES)
      {
	cycle_relax();
      }
  }

  INLINED CONFLICT_TYPE
  check_aborted()
  {
    return (*cm_abort_flag_mine != NO_CONFLICT);
    /* return arch_atomic_exchange(cm_abort_flag_mine, NO_CONFLICT); */
  }

  INLINED CONFLICT_TYPE
  get_abort_reason()
  {
    return (*cm_abort_flag_mine);	
  }

  INLINED void
  set_tx_running()
  {
    /* arch_atomic_exchange(cm_abort_flag_mine, NO_CONFLICT); */
    *cm_abort_flag_mine = NO_CONFLICT;
    /* tmc_mem_fence(); */
  }

  INLINED void
  set_tx_committed()
  {
    /* arch_atomic_exchange(cm_abort_flag_mine, TX_COMMITED); */
    *cm_abort_flag_mine = TX_COMMITED;
  }

  INLINED CONFLICT_TYPE
  set_tx_persisting()
  {
    return arch_atomic_bool_compare_and_exchange(cm_abort_flag_mine, NO_CONFLICT, PERSISTING_WRITES);
  }
#endif	/* NOCM */


#ifdef	__cplusplus
}
#endif

#endif	/* SYS_TILERA_H */

