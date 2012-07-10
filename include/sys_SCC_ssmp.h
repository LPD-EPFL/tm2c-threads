/* 
 * File:   sys_SCC_ssmp.h
 * Author: trigonak
 *
 * Created on May 16 2012, 5:07 PM
 */

#ifndef SYS_SCC_SSMP_H;
#define	SYS_SCC_SSMP_H

#include <ssmp.h>

#include "common.h"
#include "messaging.h"
#include "RCCE.h"
#ifdef PGAS
/*
 * Under PGAS we're using fakemem allocator, to have fake allocations, that
 * mimic those of RCCE_shmalloc
 */
#include "fakemem.h"
#endif

#define BARRIER  ssmp_barrier_wait(1);
#define BARRIERW ssmp_barrier_wait(0);

#ifdef	__cplusplus
extern "C" {
#endif

extern tm_addr_t shmem_start_address;
extern nodeid_t *dsl_nodes;
#define PS_BUFFER_SIZE 32
    
INLINED nodeid_t
NODE_ID(void)
{
	return (nodeid_t)RCCE_ue();
}

INLINED nodeid_t
TOTAL_NODES(void)
{
	return (nodeid_t)RCCE_num_ues();
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


INLINED tm_addr_t
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
  ssmp_sendl(to, (ssmp_msg_t *) data, len/sizeof(int));
  return 1;
}

INLINED int
sys_sendcmd_all(void* data, size_t len)
{
  int target;
  for (target = 0; target < NUM_DSL_NODES; target++) {
    ssmp_sendl(dsl_nodes[target], (ssmp_msg_t *) data, len/sizeof(int));
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


#ifdef	__cplusplus
}
#endif

#endif	/* SYS_SCC_SSMP_H */

