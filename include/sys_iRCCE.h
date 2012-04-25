/* 
 * File:   sys_iRCCE.h
 * Author: trigonak
 *
 * Created on April 25, 2012, 5:07 PM
 */

#ifndef SYS_IRCCE_H
#define	SYS_IRCCE_H

#include "common.h"
#include "messaging.h"


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
	if (len > PS_BUFFER_SIZE) {
		return 0;
	}
	char buf[PS_BUFFER_SIZE];
	memcpy(buf, data, len);
	return (iRCCE_isend(buf, PS_BUFFER_SIZE, to, NULL) == iRCCE_SUCCESS);
}

INLINED int
sys_sendcmd_all(void* data, size_t len)
{
	char buf[PS_BUFFER_SIZE];
	memcpy(buf, data, len);
	int res = 1;
	nodeid_t to;
	for (to=0; to < NUM_DSL_NODES; to++) {
		res = res 
			&& (iRCCE_isend(buf, PS_BUFFER_SIZE, dsl_nodes[to], NULL) == iRCCE_SUCCESS);
	}
	return res;
}

INLINED int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
	int res = iRCCE_irecv(data, len, from, NULL);
	return (res == RCCE_SUCCESS);
}

INLINED double
wtime(void)
{
	return RCCE_wtime();
}


#ifdef	__cplusplus
}
#endif

#endif	/* SYS_IRCCE_H */

