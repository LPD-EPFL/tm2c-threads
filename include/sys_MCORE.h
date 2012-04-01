#ifndef _SYS_MCORE_H_
#define _SYS_MCORE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <task.h>

#include "common.h"
#include "messaging.h"

EXINLINED void app_barrier();
EXINLINED void global_barrier();
#define BARRIER  app_barrier();
#define BARRIERW global_barrier();

extern int nodes_sockets[MAX_NODES]; // holds the sockets (both app and dsl
extern nodeid_t MY_NODE_ID;
extern nodeid_t MY_TOTAL_NODES;
extern Rendez* got_message; // tasks sync on this var, when there is a new msg
extern PS_REPLY* ps_remote_msg; // holds the received msg

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
	assert((to<TOTAL_NODES()));
	assert(nodes_sockets[to]!=-1);

	PRINTD("sys_sendcmd: to: %u\n", to);

	size_t error = 0;
	while (error < len) {
		error = fdwrite(nodes_sockets[to], data, len);
	}

	PRINTD("sys_sendcmd: sent %d", error);
	return (error==len)?0:-1;
}

INLINED int
sys_sendcmd_all(void* data, size_t len)
{
	int rc = 0;

	nodeid_t to;
	for (to=0; to < TOTAL_NODES(); to++) {
		if (to % DSLNDPERNODES == 0) {
			PRINTD("sys_sendcmd_all: to = %u, rc = %d", to, rc);
			sys_sendcmd(data, len, to);
		}
	}

	return rc;
}

INLINED int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
	PRINTD("sys_recvcmd: from = %u", from);
	(void)(from);

	tasksleep(got_message);
	memcpy(data, (void*)ps_remote_msg, len);
	ps_remote_msg = NULL;

	return 0;
}

INLINED tm_intern_addr_t
to_intern_addr(tm_addr_t addr)
{
#ifdef PGAS
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

#endif
