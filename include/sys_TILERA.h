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

  extern tm_addr_t shmem_start_address;
  extern nodeid_t *dsl_nodes;
#define PS_BUFFER_SIZE 32
  extern  DynamicHeader *udn_header; //headers for messaging
  extern uint32_t* demux_tags;
  extern  tmc_sync_barrier_t *barrier_apps, *barrier_all; //BARRIERS


  INLINED nodeid_t
  NODE_ID(void) {
    return (nodeid_t) ID;
  }

  INLINED nodeid_t
  TOTAL_NODES(void) {
    return (nodeid_t) NUM_UES;
  }

  INLINED tm_intern_addr_t
  to_intern_addr(tm_addr_t addr) {
#ifdef PGAS
    return fakemem_offset((void*) addr);
#else
    return ((tm_intern_addr_t) ((uintptr_t) addr - (uintptr_t) shmem_start_address));
#endif
  }

  INLINED tm_addr_t
  to_addr(tm_intern_addr_t i_addr) {
#ifdef PGAS
    return fakemem_addr_from_offset(i_addr);
#else
    return (tm_addr_t) ((uintptr_t) shmem_start_address + i_addr);
#endif
  }

  INLINED int
  sys_sendcmd(void* data, size_t len, nodeid_t to)
  {
     /* tmc_udn_send_buffer(udn_header[to], UDN0_DEMUX_TAG, data, len/sizeof(int_reg_t)); */
    tmc_udn_send_buffer(udn_header[to], demux_tags[to], data, len/sizeof(int_reg_t));
  }


  INLINED int
  sys_sendcmd_all(void* data, size_t len)
  {
    int target;
    for (target = 0; target < NUM_DSL_NODES; target++)
      {
	/* tmc_udn_send_buffer(udn_header[dsl_nodes[target]], UDN0_DEMUX_TAG, data, len/sizeof(int_reg_t)); */
	nodeid_t dsl_id = dsl_nodes[target];
	tmc_udn_send_buffer(udn_header[dsl_id], demux_tags[dsl_id], data, len/sizeof(int_reg_t));
      }
    return 1;
  
  }

  INLINED int
  sys_recvcmd(void* data, size_t len, nodeid_t from)
  {
    /* tmc_udn0_receive_buffer(data, len/sizeof(int_reg_t)); */
    switch (demux_tags[from])
      {
      case UDN0_DEMUX_TAG:
	tmc_udn0_receive_buffer(data, len/sizeof(int_reg_t));
	break;
      case UDN1_DEMUX_TAG:
	tmc_udn1_receive_buffer(data, len/sizeof(int_reg_t));
	break;
      case UDN2_DEMUX_TAG:
	tmc_udn2_receive_buffer(data, len/sizeof(int_reg_t));
	break;
      default:			/* 3 */
	tmc_udn3_receive_buffer(data, len/sizeof(int_reg_t));
	break;
      }
  }

  INLINED double
  wtime(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (double) t.tv_sec + ((double) t.tv_usec) / 1000000.0;
  }


#ifdef	__cplusplus
}
#endif

#endif	/* SYS_TILERA_H */

