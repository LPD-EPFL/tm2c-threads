/* 
 * File:   pubSubTM.c
 * Author: trigonak
 *
 * Created on March 7, 2011, 4:45 PM
 * 
 */

#include "common.h"
#include "pubSubTM.h"

nodeid_t *dsl_nodes; // holds the ids of the nodes. ids are in range 0..48 (possibly more)
// To get the address of the node, one must call id_to_addr

unsigned short nodes_contacted[48];
CONFLICT_TYPE ps_response; //TODO: make it more sophisticated

PS_COMMAND *psc;

int read_value;

static inline void ps_sendb(nodeid_t target, PS_COMMAND_TYPE operation,
                            tm_intern_addr_t address, CONFLICT_TYPE response);
static inline void ps_sendbv(nodeid_t target, PS_COMMAND_TYPE operation,
                            tm_intern_addr_t address, uint32_t value,
                            CONFLICT_TYPE response);
static inline void ps_recvb(nodeid_t from);

inline void unsubscribe(nodeid_t nodeId, tm_addr_t shmem_address);

/*
 * Takes the local representation of the address, and finds the node
 * responsible for it.
 */
static inline nodeid_t get_responsible_node(tm_intern_addr_t addr);

inline void publish_finish(nodeid_t nodeId, tm_addr_t shmem_address);

void ps_init_(void) {
    PRINTD("NUM_DSL_NODES = %d", NUM_DSL_NODES);
    if ((dsl_nodes = (unsigned int *) malloc(NUM_DSL_NODES * sizeof (unsigned int))) == NULL) {
        PRINT("malloc dsl_nodes");
        EXIT(-1);
    }

    psc = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    if (psc == NULL) {
        PRINT("malloc psc == NULL");
    }

    int dsln = 0;
    unsigned int j;
    for (j = 0; j < NUM_UES; j++) {
        nodes_contacted[j] = 0;
        if (j % DSLNDPERNODES == 0) {
            dsl_nodes[dsln++] = j;
        }
    }

    sys_ps_init_();
    PRINT("[APP NODE] Initialized pub-sub..");
}

#ifdef PGAS

static inline void
ps_send_rl(nodeid_t target, tm_intern_addr_t address)
{
#ifdef PLATFORM_CLUSTER
	psc->nodeId = ID;
#endif
    psc->type = PS_SUBSCRIBE;
    psc->address = address;

    sys_sendcmd(psc, sizeof(PS_COMMAND), target);
}

#endif /* PGAS */

static inline void 
ps_sendb(nodeid_t target, PS_COMMAND_TYPE command,
         tm_intern_addr_t address, CONFLICT_TYPE response)
{
#ifdef PLATFORM_CLUSTER
	psc->nodeId = ID;
#endif
    psc->type = command;
    psc->address = address;
    psc->response = response;

    sys_sendcmd(psc, sizeof(PS_COMMAND), target);
}

static inline void 
ps_sendbv(nodeid_t target, PS_COMMAND_TYPE command,
         tm_intern_addr_t address, uint32_t value,
         CONFLICT_TYPE response)
{
#ifdef PLATFORM_CLUSTER
	psc->nodeId = ID;
#endif
    psc->type = command;
    psc->address = address;
    psc->response = response;
    psc->write_value = value;

    sys_sendcmd(psc, sizeof(PS_COMMAND), target);
}

static inline void
ps_recvb(nodeid_t from)
{
    // XXX: this could be written much better, without globals
    PS_COMMAND cmd; 

    sys_recvcmd(&cmd, sizeof(PS_COMMAND), from);
    ps_response = cmd.response;
#ifdef PGAS
    PF_START(0)
    read_value = cmd.value;
    PF_STOP(0)
#endif
}

/*
 * ____________________________________________________________________________________________
 TM interface _________________________________________________________________________________|
 * ____________________________________________________________________________________________|
 */

CONFLICT_TYPE
ps_subscribe(tm_addr_t address)
{
	tm_intern_addr_t intern_addr = to_intern_addr(address);

    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]++;

    //PF_START(1)
#ifdef PGAS
    //ps_send_rl(responsible_node, (unsigned int) address);
    ps_sendbv(responsible_node, PS_SUBSCRIBE, intern_addr, 0, NO_CONFLICT);
#else
    //PF_START(2)
    ps_sendb(responsible_node, PS_SUBSCRIBE, intern_addr, NO_CONFLICT);
    //PF_STOP(2)
#endif
    //    PRINTD("[SUB] addr: %d to %02d", address_offs, responsible_node);
    //PF_START(3)
    ps_recvb(responsible_node);
    //PF_STOP(3)
    // PF_STOP(1)

    return ps_response;
}

#ifdef PGAS

CONFLICT_TYPE ps_publish(tm_addr_t address, int value) {
#else

CONFLICT_TYPE ps_publish(tm_addr_t address) {
#endif

	tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]++;

#ifdef PGAS
    ps_sendbv(responsible_node, PS_PUBLISH, intern_addr, value, NO_CONFLICT);
#else
    ps_sendb(responsible_node, PS_PUBLISH, intern_addr, NO_CONFLICT); //make sync
#endif
    ps_recvb(responsible_node);

    return ps_response;
}

#ifdef PGAS

CONFLICT_TYPE ps_store_inc(tm_addr_t address, int increment) {
	tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]++;

    ps_sendbv(responsible_node, PS_WRITE_INC, intern_addr, increment, NO_CONFLICT);
    ps_recvb(responsible_node);

    return ps_response;
}
#endif

void ps_unsubscribe(tm_addr_t address) {
	tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]--;

    ps_sendb(responsible_node, PS_UNSUBSCRIBE, intern_addr, NO_CONFLICT);

#ifdef PLATFORM_CLUSTER
	ps_recvb(responsible_node);
#endif
}

void ps_publish_finish(tm_addr_t address) {
	tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]--;

    ps_sendb(responsible_node, PS_PUBLISH_FINISH, intern_addr, NO_CONFLICT);

#ifdef PLATFORM_CLUSTER
	ps_recvb(responsible_node);
#endif
}

void ps_finish_all(CONFLICT_TYPE conflict) {
#define FINISH_ALL_PARALLEL_
#ifdef FINISH_ALL_PARALLEL
    iRCCE_SEND_REQUEST sends[NUM_UES];
    char data[PS_BUFFER_SIZE];
    psc->type = PS_REMOVE_NODE;
    psc->response = conflict;
    memcpy(data, psc, sizeof (PS_COMMAND));
#endif

    nodeid_t i;
    for (i = 0; i < NUM_UES; i++) {
        if (nodes_contacted[i] != 0) { //can be changed to non-blocking

#ifndef FINISH_ALL_PARALLEL
            ps_sendb(i, PS_REMOVE_NODE, 0, conflict);
#ifdef PLATFORM_CLUSTER
			// need a dummy receive, due to the way how ZMQ works
			ps_recvb(i);
#endif
#else
            if (iRCCE_isend(data, PS_BUFFER_SIZE, i, &sends[i]) != iRCCE_SUCCESS) {
                iRCCE_add_send_to_wait_list(&waitlist, &sends[i]);
            }
#endif
            nodes_contacted[i] = 0;
        }
    }

#ifdef FINISH_ALL_PARALLEL
    iRCCE_wait_all(&waitlist);
#endif

}

void ps_send_stats(stm_tx_node_t* stats, double duration) {
    psc->type = PS_STATS;

    psc->aborts = stats->tx_aborted;
    psc->aborts_raw = stats->aborts_raw;
    psc->aborts_war = stats->aborts_war;
    psc->aborts_waw = stats->aborts_waw;
    psc->commits = stats->tx_commited;
    psc->max_retries = stats->max_retries;
    psc->tx_duration = duration;

    sys_sendcmd_all(psc, sizeof(PS_COMMAND));
}

static inline nodeid_t
get_responsible_node(tm_intern_addr_t addr)
{
#ifdef PGAS
	nodeid_t node = (nodeid_t)(addr % NUM_DSL_NODES);
    return dsl_nodes[node];
#else
    /* shift right by DHT_ADDRESS_MASK, thus making 2^DHT_ADDRESS_MASK continuous
        address handled by the same node*/
    return dsl_nodes[((addr) >> DHT_ADDRESS_MASK) % NUM_DSL_NODES];
#endif
}
