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
#  include "tm.h"
#endif

#include <limits.h>
#include <assert.h>
#include "hash.h"
#ifdef PGAS
#include "pgas_app.h"
#include "pgas_dsl.h"
#endif


//#define LOG_LATENCIES
#ifdef LOG_LATENCIES
#  include <sys/time.h>
   struct timeval before;
   struct timeval after;
#endif

nodeid_t* dsl_nodes; // holds the ids of the nodes. ids are in range 0..48 (possibly more)
// To get the address of the node, one must call id_to_addr

unsigned short nodes_contacted[48];

PS_COMMAND *psc;


int64_t read_value;

unsigned long int* tm2c_rand_seeds;

static inline void ps_sendb(nodeid_t target, PS_COMMAND_TYPE operation,
        tm_intern_addr_t address);
static inline void ps_sendbr(nodeid_t target, PS_COMMAND_TYPE operation,
        tm_intern_addr_t address, CONFLICT_TYPE response);
static inline void ps_sendbv(nodeid_t target, PS_COMMAND_TYPE operation,
        tm_intern_addr_t address, int64_t value);
static inline CONFLICT_TYPE ps_recvb(nodeid_t from);

/*
 * Takes the local representation of the address, and finds the node
 * responsible for it.
 */
static inline nodeid_t get_responsible_node(tm_intern_addr_t addr);

static unsigned long* 
seed_rand() 
{
  unsigned long* seeds;
  seeds = (unsigned long*) malloc(3 * sizeof(unsigned long));
  assert(seeds != NULL);
  seeds[0] = getticks() % 123456789;
  seeds[1] = getticks() % 362436069;
  seeds[2] = getticks() % 521288629;
  return seeds;
}


void
ps_init_(void) 
{
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
  for (j = 0; j < NUM_UES; j++) 
    {
      nodes_contacted[j] = 0;
      if (!is_app_core(j)) 
	{
	  dsl_nodes[dsln++] = j;
	}
    }

  tm2c_rand_seeds = seed_rand();
  sys_ps_init_();
  /* PRINT("[APP NODE] Initialized pub-sub.."); */
}

static inline void
ps_sendb(nodeid_t target, PS_COMMAND_TYPE command,
         tm_intern_addr_t address)
{
  psc->type = command;
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_TILERA) || defined(PLATFORM_MCORE_SHRIMP)
  psc->nodeId = ID;
#endif
  psc->address = address;

#if defined(WHOLLY)
  psc->tx_metadata = stm_tx_node->tx_commited;
#elif defined(FAIRCM)
  psc->tx_metadata = stm_tx_node->tx_duration;
#elif defined(GREEDY)
#  ifdef GREEDY_GLOBAL_TS
  psc->tx_metadata = stm_tx->start_ts;
#  else
  psc->tx_metadata = getticks() - stm_tx->start_ts;
#  endif
#endif
  sys_sendcmd(psc, sizeof(PS_COMMAND), target);

}

static inline void
ps_sendbr(nodeid_t target, PS_COMMAND_TYPE command,
         tm_intern_addr_t address, CONFLICT_TYPE response)
{
    psc->type = command;
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_TILERA) || defined(PLATFORM_MCORE_SHRIMP)
    psc->nodeId = ID;
#endif
    psc->address = address;
#if defined(PGAS)
    psc->response = response;
#endif	/* PGAS */
    sys_sendcmd(psc, sizeof (PS_COMMAND), target);
}

static inline void
ps_sendbv(nodeid_t target, PS_COMMAND_TYPE command,
         tm_intern_addr_t address, int64_t value)
{
    psc->type = command;
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_TILERA) || defined(PLATFORM_MCORE_SHRIMP)
	psc->nodeId = ID;
#endif
    psc->address = address;
    
#if defined(PGAS)
    psc->write_value = value;
#endif	/* PGAS */
    sys_sendcmd(psc, sizeof(PS_COMMAND), target);
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
ps_recvb(nodeid_t from)
{
#if defined(PLATFORM_MCORE) || defined(PLATFORM_TILERA) || defined(PLATFORM_SCC_SSMP)
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
ps_subscribe(tm_addr_t address, int words) 
{
  tm_intern_addr_t intern_addr = to_intern_addr(address);

  nodeid_t responsible_node_seq = get_responsible_node(intern_addr);

  nodeid_t responsible_node = nodes_contacted[responsible_node_seq]++;
  responsible_node = dsl_nodes[responsible_node_seq];

#ifdef PGAS
  intern_addr &= PGAS_DSL_ADDR_MASK;
  ps_sendbv(responsible_node, PS_SUBSCRIBE, intern_addr, words);
#else
  ps_sendb(responsible_node, PS_SUBSCRIBE, intern_addr);
#endif

  CONFLICT_TYPE response = ps_recvb(responsible_node);
  if (response != NO_CONFLICT)
    {
      nodes_contacted[responsible_node_seq] = 0;
    }

  return response;
}


#ifdef PGAS
CONFLICT_TYPE ps_publish(tm_addr_t address, int64_t value) {
#else
  CONFLICT_TYPE ps_publish(tm_addr_t address) {
#endif

    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node_seq = get_responsible_node(intern_addr);

    nodes_contacted[responsible_node_seq]++;
    nodeid_t responsible_node = responsible_node = dsl_nodes[responsible_node_seq];

#ifdef PGAS
    intern_addr &= PGAS_DSL_ADDR_MASK;
    ps_sendbv(responsible_node, PS_PUBLISH, intern_addr, value);
#else
    ps_sendb(responsible_node, PS_PUBLISH, intern_addr); //make sync
#endif

    CONFLICT_TYPE response = ps_recvb(responsible_node);
    if (response != NO_CONFLICT)
      {
    	nodes_contacted[responsible_node_seq] = 0;
      }

    return response;
  }


#ifdef PGAS
  CONFLICT_TYPE ps_store_inc(tm_addr_t address, int64_t increment) 
  {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

    intern_addr &= PGAS_DSL_ADDR_MASK;

    nodes_contacted[responsible_node]++;
    responsible_node = dsl_nodes[responsible_node];

    ps_sendbv(responsible_node, PS_WRITE_INC, intern_addr, increment);

    CONFLICT_TYPE response = ps_recvb(responsible_node);
    if (response != NO_CONFLICT)
      {
    	nodes_contacted[responsible_node] = 0;
      }

    return response;
  }
#endif	/* PGAS */

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
  /*       /\* wait_cycles(150 * stm_tx->retries); *\/ */
  /*     } */
  /*   while (1); */
  /*   //  CM_METADATA_UPDATE_ON_COMMIT; */
  /*   return (response == CAS_SUCCESS); */
  /* } */

  uint64_t
    ps_load(tm_addr_t address, int words) 
  {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);
    responsible_node = dsl_nodes[responsible_node];

#if defined(PGAS)
    intern_addr &= PGAS_DSL_ADDR_MASK;
#endif	/* PGAS */

    ps_sendbr(responsible_node, PS_LOAD_NONTX, intern_addr, words);
    ps_recvb(responsible_node);

    return read_value;
  }

  void
    ps_store(tm_addr_t address, int64_t value) 
  {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);
    responsible_node = dsl_nodes[responsible_node];

#if defined(PGAS)
    intern_addr &= PGAS_DSL_ADDR_MASK;
#endif	/* PGAS */

    ps_sendbv(responsible_node, PS_STORE_NONTX, intern_addr, value);
#ifdef USING_ZMQ
    ps_recvb(responsible_node);
#endif
  }

  void ps_unsubscribe(tm_addr_t address) 
  {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);
#if defined(PGAS)
    intern_addr &= PGAS_DSL_ADDR_MASK;
#endif	/* PGAS */

    nodes_contacted[responsible_node]--;
    responsible_node = dsl_nodes[responsible_node];


    ps_sendb(responsible_node, PS_UNSUBSCRIBE, intern_addr);

#ifdef PLATFORM_CLUSTER
    ps_recvb(responsible_node);
#endif
  }

  void ps_publish_finish(tm_addr_t address) 
  {
    tm_intern_addr_t intern_addr = to_intern_addr(address);
    nodeid_t responsible_node = get_responsible_node(intern_addr);

#if defined(PGAS)
    intern_addr &= PGAS_DSL_ADDR_MASK;
#endif	/* PGAS */

    nodes_contacted[responsible_node]--;
    ps_sendb(responsible_node, PS_PUBLISH_FINISH, intern_addr);

#ifdef PLATFORM_CLUSTER
    ps_recvb(responsible_node);
#endif
  }

  void 
    ps_finish_all(CONFLICT_TYPE conflict) 
  {
    nodeid_t i;
    for (i = 0; i < NUM_DSL_NODES; i++) 
      {
	if (nodes_contacted[i] > 0) 
	  { 
	    ps_sendbr(dsl_nodes[i], PS_REMOVE_NODE, 0, conflict);
#ifdef PLATFORM_CLUSTER
	    ps_recvb(i);	    // need a dummy receive, due to the way how ZMQ works
#endif
	    nodes_contacted[i] = 0;
	  }
      }
  }

  void
    ps_send_stats(stm_tx_node_t* stats, double duration)
  {
    PS_STATS_CMD_T* stats_cmd = (PS_STATS_CMD_T*) malloc(sizeof(PS_STATS_CMD_T));
    if (stats_cmd == NULL)
      {
	PRINT("malloc @ ps_send_stats");
	stats_cmd = (PS_STATS_CMD_T*) psc;
      }

    stats_cmd->type = PS_STATS;
    stats_cmd->nodeId = NODE_ID();

    stats_cmd->aborts = stats->tx_aborted;
    stats_cmd->commits = stats->tx_commited;
    stats_cmd->max_retries = stats->max_retries;
    stats_cmd->tx_duration = duration;

    sys_sendcmd_all(stats_cmd, sizeof(PS_STATS_CMD_T));

    stats_cmd->aborts_raw = stats->aborts_raw;
    stats_cmd->aborts_war = stats->aborts_war;
    stats_cmd->aborts_waw = stats->aborts_waw;
    stats_cmd->tx_duration = 0;
    
    sys_sendcmd_all(stats_cmd, sizeof(PS_STATS_CMD_T));

    free(stats_cmd);

    BARRIERW;
  }

  CONFLICT_TYPE
    ps_dummy_msg(nodeid_t node)
  {
    node = dsl_nodes[node];
    /* PF_START(1); */
    ps_sendb(node, PS_UKNOWN, 0);
    /* PF_STOP(1); */
    /* PF_START(2); */
    CONFLICT_TYPE response = ps_recvb(node);
    /* PF_STOP(2); */
    return response;
  }


  static inline nodeid_t
    get_responsible_node(tm_intern_addr_t addr) 
  {
#ifdef USE_ARRAY
    return dsl_nodes[((addr) >> 4) % NUM_DSL_NODES];
#else
    //    unsigned int hash_val = hash_tw((addr>>2) % UINT_MAX);
    ///    return dsl_nodes[hash_val % NUM_DSL_NODES];
    /* shift right by RESP_NODE_MASK, thus making 2^RESP_NODE_MASK continuous
       address handled by the same node*/

#  ifndef PGAS
    /* return dsl_nodes[((addr) >> RESP_NODE_MASK) % NUM_DSL_NODES]; */
    /* return dsl_nodes[(hash_tw(addr) >> RESP_NODE_MASK) % NUM_DSL_NODES]; */
    /* return dsl_nodes[(hash_tw(addr)) % NUM_DSL_NODES]; */
    return (hash_tw(addr)) % NUM_DSL_NODES;
#  else	 /* PGAS */
    /* return dsl_nodes[addr / pgas_dsl_size_node]; */
    return dsl_nodes[addr >> PGAS_DSL_MASK_BITS];
#  endif  /* PGAS */

#endif
  }
