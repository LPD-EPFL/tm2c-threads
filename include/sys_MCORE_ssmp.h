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

extern nodeid_t MY_NODE_ID;
extern nodeid_t MY_TOTAL_NODES;

extern PS_REPLY* ps_remote_msg; // holds the received msg
extern nodeid_t *dsl_nodes;

/* --- inlined methods --- */
INLINED nodeid_t
NODE_ID(void)
{
	return MY_NODE_ID;
}


INLINED nodeid_t
TOTAL_NODES(void)
{
	return MY_TOTAL_NODES;
}

INLINED int
sys_sendcmd(void* data, size_t len, nodeid_t to)
{
  //  PS_COMMAND *cmd = (PS_COMMAND *) data;

  ssmp_send(to, (ssmp_msg_t *) data, sizeof(PS_COMMAND));
}

INLINED int
sys_sendcmd_all(void* data, size_t len)
{
  PS_COMMAND *cmd = (PS_COMMAND *) data;

   int target;
  for (target = 0; target < NUM_DSL_NODES; target++) {
    ssmp_send(dsl_nodes[target], (ssmp_msg_t *) data, sizeof(PS_COMMAND));
  }
  return 1;
}

INLINED int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
  PS_COMMAND *cmd = (PS_COMMAND *) data;
  ssmp_recv_from(from, (ssmp_msg_t *) data, sizeof(PS_COMMAND));

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


#endif
