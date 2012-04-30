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
    PS_COMMAND *cmd = (PS_COMMAND *) data;

    if (cmd->type != PS_REMOVE_NODE) {
#ifdef PGAS
        if (cmd->type == PS_WRITE_INC || cmd->type == PS_PUBLISH || cmd->type == PS_STORE_NONTX) {
            tmc_udn_send_4(udn_header[to], UDN0_DEMUX_TAG, ID, cmd->type, cmd->address, cmd->write_value);
            return 1;
        }
#endif
	// not pgas or not a WRITE cmd
        tmc_udn_send_3(udn_header[to], UDN0_DEMUX_TAG, ID, cmd->type, cmd->address);
    }
    else { /* PS_REMOVE_NODE */
#ifndef PGAS
        tmc_udn_send_2(udn_header[to], UDN0_DEMUX_TAG, ID, cmd->type);
#else   //PGAS
        tmc_udn_send_3(udn_header[to], UDN0_DEMUX_TAG, ID, cmd->type, cmd->response);
#endif
    }
  }

  INLINED int
  sys_sendcmd_all(void* data, size_t len)
  {
      PS_COMMAND *cmd = (PS_COMMAND *) data;

    union {
        int from[2];
        double to;
    } convert;
    convert.to = cmd->tx_duration;

    int target;
    for (target = 0; target < NUM_DSL_NODES; target++) {
        tmc_udn_send_10(udn_header[dsl_nodes[target]], UDN0_DEMUX_TAG, ID, PS_STATS,
                cmd->aborts, cmd->aborts_raw, cmd->aborts_war,
                cmd->aborts_waw, cmd->commits, convert.from[0],
                convert.from[1], cmd->max_retries);
    }
    return 1;
  }

  INLINED int
  sys_recvcmd(void* data, size_t len, nodeid_t from)
  {
    PS_COMMAND *cmd = (PS_COMMAND *) data;
    cmd->response = tmc_udn0_receive();
#ifdef PGAS
    cmd->value = tmc_udn0_receive();
#endif
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

