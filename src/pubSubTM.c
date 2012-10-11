/* 
 * File:   pubSubTM.c
 * Author: trigonak
 *
 * Created on March 7, 2011, 4:45 PM
 * 
 */

#include "common.h"
#include "pubSubTM.h"
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
#include "tm.h"
#endif

#include <limits.h>
#include "hash.h"

//#define LOG_LATENCIES
#ifdef LOG_LATENCIES
#include <sys/time.h>
   struct timeval before;
   struct timeval after;
#endif

nodeid_t *dsl_nodes; // holds the ids of the nodes. ids are in range 0..48 (possibly more)
// To get the address of the node, one must call id_to_addr

unsigned short nodes_contacted[48];

PS_COMMAND *psc;

int read_value;

static inline void ps_sendb(nodeid_t target, PS_COMMAND_TYPE operation,
        tm_intern_addr_t address);
static inline void ps_sendbr(nodeid_t target, PS_COMMAND_TYPE operation,
        tm_intern_addr_t address, CONFLICT_TYPE response);
static inline void ps_sendbv(nodeid_t target, PS_COMMAND_TYPE operation,
        tm_intern_addr_t address, uint32_t value,
        CONFLICT_TYPE response);
static inline CONFLICT_TYPE ps_recvb(nodeid_t from);

/*
 * Takes the local representation of the address, and finds the node
 * responsible for it.
 */
static inline nodeid_t get_responsible_node(tm_intern_addr_t addr);

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
        if (!is_app_core(j)) {
            dsl_nodes[dsln++] = j;
        }
    }

    sys_ps_init_();
    PRINT("[APP NODE] Initialized pub-sub..");
}

static inline void
ps_sendb(nodeid_t target, PS_COMMAND_TYPE command,
         tm_intern_addr_t address)
{
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_TILERA) || defined(PLATFORM_MCORE_SHRIMP)
	psc->nodeId = ID;
#endif
    psc->type = command;
    psc->address = address;
    //psc->response = response;

#if defined(WHOLLY)
    psc->tx_metadata = stm_tx_node->tx_commited;
#elif defined(FAIRCM)
    psc->tx_metadata = stm_tx_node->tx_duration;
#elif defined(GREEDY)
    psc->tx_metadata = getticks() - stm_tx->start_ts;
#endif
    sys_sendcmd(psc, sizeof (PS_COMMAND), target);

}

static inline void
ps_sendbr(nodeid_t target, PS_COMMAND_TYPE command,
         tm_intern_addr_t address, CONFLICT_TYPE response)
{
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_TILERA) || defined(PLATFORM_MCORE_SHRIMP)
	psc->nodeId = ID;
#endif
    psc->type = command;
    psc->address = address;
    psc->response = response;
    sys_sendcmd(psc, sizeof (PS_COMMAND), target);

}

static inline void
ps_sendbv(nodeid_t target, PS_COMMAND_TYPE command,
         tm_intern_addr_t address, uint32_t value,
         CONFLICT_TYPE response)
{
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_TILERA) || defined(PLATFORM_MCORE_SHRIMP)
	psc->nodeId = ID;
#endif
    psc->type = command;
    psc->address = address;
    psc->response = response;
    psc->write_value = value;

    sys_sendcmd(psc, sizeof (PS_COMMAND), target);
#ifdef LOG_LATENCIES
   gettimeofday(&after,NULL);
   double t2 = (double) after.tv_sec + after.tv_usec / 1e6f;
   fprintf(stderr, "%d sent at %f\n",getpid(),t2);
#endif

}

/* static inline void */
/* ps_send_cas(nodeid_t target, PS_COMMAND_TYPE command, tm_intern_addr_t address, */
/* 	    uint32_t oldval, uint32_t newval) */
/* { */
/* #if defined(PLATFORM_CLUSTER) || defined(PLATFORM_TILERA) */
/*   psc->nodeId = ID; */
/* #endif */
/*   psc->type = command; */
/*   psc->oldval = oldval; */
/*   psc->address = address; */
/*   psc->write_value = newval; */

/* #if defined(WHOLLY) */
/*   psc->tx_metadata = stm_tx_node->tx_commited; */
/* #elif defined(FAIRCM) */
/*   psc->tx_metadata = stm_tx_node->tx_duration; */
/* #elif defined(GREEDY) */
/*   psc->tx_metadata = getticks() - stm_tx->start_ts; */
/* #endif */
/*   sys_sendcmd(psc, sizeof (PS_COMMAND), target); */

/* } */


static inline CONFLICT_TYPE
ps_recvb(nodeid_t from) {
#if defined(PLATFORM_MCORE) || defined(PLATFORM_TILERA)
    PS_REPLY cmd;
    sys_recvcmd(&cmd, sizeof (PS_REPLY), from);
#else
    PS_COMMAND cmd;
    sys_recvcmd(&cmd, sizeof (PS_COMMAND), from);
#endif
#ifdef PGAS
    read_value = cmd.value;
#endif
	return cmd.response;
}

/*
 * ____________________________________________________________________________________________
 TM interface _________________________________________________________________________________|
 * ____________________________________________________________________________________________|
 */

CONFLICT_TYPE
ps_subscribe(tm_addr_t address) {
    tm_intern_addr_t intern_addr = to_intern_addr(address);

    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]++;


#ifdef PGAS
    //ps_send_rl(responsible_node, (unsigned int) address);
    ps_sendbv(responsible_node, PS_SUBSCRIBE, intern_addr, 0, NO_CONFLICT);
#else
    ps_sendb(responsible_node, PS_SUBSCRIBE, intern_addr);
#endif

    CONFLICT_TYPE response = ps_recvb(responsible_node);

    return response;
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
    ps_sendb(responsible_node, PS_PUBLISH, intern_addr); //make sync
#endif
    CONFLICT_TYPE response = ps_recvb(responsible_node);
    return response;
}

#ifdef PGAS

CONFLICT_TYPE ps_store_inc(tm_addr_t address, int increment) {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]++;

    ps_sendbv(responsible_node, PS_WRITE_INC, intern_addr, (uint32_t) increment, NO_CONFLICT);

    return ps_recvb(responsible_node);
}
#endif

/* inline uint32_t  */
/* tx_casi(tm_addr_t addr, uint32_t oldval, uint32_t newval) */
/* { */
/*   CONFLICT_TYPE response; */
/*   //  stm_tx->retries = 0; */
/*   //  CM_METADATA_INIT_ON_FIRST_START; */
/*   do */
/*     { */
/*       //      CM_METADATA_INIT_ON_START; */
/*       //      stm_tx->retries++; */
      
/*       tm_intern_addr_t intern_addr = to_intern_addr(addr); */
/*       nodeid_t responsible_node = get_responsible_node(intern_addr); */

/*       ps_send_cas(responsible_node, PS_CAS, intern_addr, oldval, newval); */
/*       response = ps_recvb(responsible_node); */
/*       if (response == NO_CONFLICT || response == CAS_SUCCESS) */
/* 	{ */
/* 	  break; */
/* 	} */
      
/*       /\* /\\* aborted *\\/ *\/ */
/*       /\* stm_tx->aborts++; *\/ */

/*       /\* switch (response) *\/ */
/*       /\* 	{ *\/ */
/*       /\* 	case READ_AFTER_WRITE: *\/ */
/*       /\* 	  stm_tx->aborts_raw++; *\/ */
/*       /\* 	  break; *\/ */
/*       /\* 	case WRITE_AFTER_READ: *\/ */
/*       /\* 	  stm_tx->aborts_war++; *\/ */
/*       /\* 	  break; *\/ */
/*       /\* 	case WRITE_AFTER_WRITE: *\/ */
/*       /\* 	  stm_tx->aborts_waw++; *\/ */
/*       /\* 	  break; *\/ */
/*       /\* 	default: *\/ */
/*       /\* 	  ; *\/ */
/*       /\* 	  /\\* nothing *\\/ *\/ */
/*       /\* 	} *\/ */

/*       /\* wait_cycles(150 * stm_tx->retries); *\/ */
/*     } */
/*   while (1); */

  
/*   //  CM_METADATA_UPDATE_ON_COMMIT; */
/*   /\* stm_tx_node->tx_starts += stm_tx->retries;                           *\/ */
/*   /\* stm_tx_node->tx_commited++;                                          *\/ */
/*   /\* stm_tx_node->tx_aborted += stm_tx->aborts;                           *\/ */
/*   /\* stm_tx_node->max_retries =                                           *\/ */
/*   /\*   (stm_tx->retries < stm_tx_node->max_retries)                       *\/ */
/*   /\*   ? stm_tx_node->max_retries  *\/ */
/*   /\*   : stm_tx->retries;                                           *\/ */
/*   /\* stm_tx_node->aborts_war += stm_tx->aborts_war;                       *\/ */
/*   /\* stm_tx_node->aborts_raw += stm_tx->aborts_raw;                       *\/ */
/*   /\* stm_tx_node->aborts_waw += stm_tx->aborts_waw;                       *\/ */

/*   /\* stm_tx = tx_metadata_empty(stm_tx); *\/ */

/*   return (response == CAS_SUCCESS); */
/* } */

uint32_t
ps_load(tm_addr_t address) {
  tm_intern_addr_t intern_addr = to_intern_addr(address);
  nodeid_t responsible_node = get_responsible_node(intern_addr);

  ps_sendb(responsible_node, PS_LOAD_NONTX, intern_addr);
  ps_recvb(responsible_node);

  return read_value;
}

void
ps_store(tm_addr_t address, uint32_t value) {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    ps_sendbv(responsible_node, PS_STORE_NONTX, intern_addr, value, NO_CONFLICT);
#ifdef USING_ZMQ
    ps_recvb(responsible_node);
#endif
}

void ps_unsubscribe(tm_addr_t address) {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]--;
    ps_sendb(responsible_node, PS_UNSUBSCRIBE, intern_addr);

#ifdef PLATFORM_CLUSTER
	ps_recvb(responsible_node);
#endif
}

void ps_publish_finish(tm_addr_t address) {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node]--;

    ps_sendb(responsible_node, PS_PUBLISH_FINISH, intern_addr);

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
            ps_sendbr(i, PS_REMOVE_NODE, 0, conflict);
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

 void
   ps_send_stats(stm_tx_node_t* stats, double duration)
 {
   psc->type = PS_STATS;

   psc->aborts = stats->tx_aborted;
   psc->commits = stats->tx_commited;
   psc->max_retries = stats->max_retries;
   psc->tx_duration = duration;

   sys_sendcmd_all(psc, sizeof (PS_COMMAND));

   psc->aborts_raw = stats->aborts_raw;
   psc->aborts_war = stats->aborts_war;
   psc->aborts_waw = stats->aborts_waw;
   psc->tx_duration = 0;
    
   sys_sendcmd_all(psc, sizeof (PS_COMMAND));

   BARRIERW;
}

 /* CONFLICT_TYPE */
 /*   ps_dummy_msg(nodeid_t node)  */
 /* { */
 /*   node = dsl_nodes[node]; */
 /*   ps_sendb(node, PS_UKNOWN, 0); */

 /*   CONFLICT_TYPE response = ps_recvb(node); */
 /*   return response; */
 /* } */

 CONFLICT_TYPE
   ps_dummy_msg(nodeid_t node)
 {
   node = dsl_nodes[node];
   PF_START(1);
   ps_sendb(node, PS_UKNOWN, 0);
   PF_STOP(1);
   PF_START(2);
   CONFLICT_TYPE response = ps_recvb(node);
   PF_STOP(2);
   return response;
 }


static inline nodeid_t
get_responsible_node(tm_intern_addr_t addr) {
#ifdef USE_ARRAY
    return dsl_nodes[((addr) >> 4) % NUM_DSL_NODES];
#else
//    unsigned int hash_val = hash_tw((addr>>2) % UINT_MAX);
///    return dsl_nodes[hash_val % NUM_DSL_NODES];
    /* shift right by RESP_NODE_MASK, thus making 2^RESP_NODE_MASK continuous
        address handled by the same node*/

    return dsl_nodes[((addr) >> RESP_NODE_MASK) % NUM_DSL_NODES];
#endif
}
