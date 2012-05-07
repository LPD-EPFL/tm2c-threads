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
  PS_COMMAND *cmd = (PS_COMMAND *) data;

  if (cmd->type != PS_REMOVE_NODE) {
#ifdef PGAS
    if (cmd->type == PS_WRITE_INC || cmd->type == PS_PUBLISH || cmd->type == PS_STORE_NONTX) {
      ssmp_send3(to, cmd->type, cmd->address, cmd->write_value);
      return 1;
    }
#endif
    // not pgas or not a WRITE cmd
    ssmp_send2(to, cmd->type, cmd->address);
  }
  else { /* PS_REMOVE_NODE */
#ifndef PGAS
    ssmp_send1(to, cmd->type);
#else   //PGAS
    ssmp_send2(to, cmd->type, cmd->response);
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
    if (convert.to) {
      ssmp_send6(dsl_nodes[target], PS_STATS,
		     convert.from[0], convert.from[1], cmd->aborts, cmd->commits,
		     cmd->max_retries);
    }
    else {
      ssmp_send6(dsl_nodes[target], PS_STATS,
		     convert.from[0], convert.from[1], 
		     cmd->aborts_raw, cmd->aborts_war, cmd->aborts_waw);
    }
  }
  return 1;
}

INLINED int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
  PS_COMMAND *cmd = (PS_COMMAND *) data;
  //XXX: make recv_from2
  ssmp_msg_t msg;
  ssmp_recv_from(from, &msg, 2);
  cmd->response = msg.w0;
#ifdef PGAS
  cmd->value = msg.w1;
#endif

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
