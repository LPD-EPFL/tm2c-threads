/* 
 * File:   tm.c
 * Author: trigonak
 *
 * Created on June 16, 2011, 12:29 PM
 */

#include <sys/param.h>

#include "tm.h"
#ifdef PGAS
#include "pgas.h"
#endif

nodeid_t ID;
nodeid_t NUM_UES;
nodeid_t NUM_DSL_NODES;
nodeid_t NUM_APP_NODES;

stm_tx_t *stm_tx = NULL;
stm_tx_node_t *stm_tx_node = NULL;

double duration__ = 0;

const char *conflict_reasons[4] = {
    "NO_CONFLICT",
    "READ_AFTER_WRITE",
    "WRITE_AFTER_READ",
    "WRITE_AFTER_WRITE"
};


#define DSL_BY_MOD
#define DSLPERNODE 3

//custom assignment
const unsigned short buf[8] = {0xAAAA,0xAAAA,0xAAAA,0xAAAA,0xAAAA,0xAAAA,0xAAAA,0xAAAA};
/* set to 1 if dsl node */
const uint8_t dsl_node[] =
  {
    1, 0, 0, 1, 0, 0,		/* 6 */
    1, 0, 0, 1, 0, 0,		/* 12 */
    1, 0, 0, 1, 0, 0,		/* 18 */
    1, 0, 0, 1, 0, 0,		/* 24 */
    1, 0, 1, 1, 0, 0,		/* 30 */
    1, 0, 0, 1, 0, 0,		/* 36 */
    1, 0, 0, 1, 0, 0,		/* 42 */
    1, 0, 0, 1, 0, 0,		/* 48 */
    0, 0, 0, 0, 0, 0,		/* ... */
    0, 0, 0, 0, 0, 0,		/*  */
    0, 0, 0, 0, 0, 0,		/*  */
    0, 0, 0, 0, 0, 0,		/*  */
  };

/*______________________________________________________________________________________________________
 * TM Interface                                                                                         |
 *______________________________________________________________________________________________________|
 */

int
is_app_core(int id)
{
  //return 0 if dsl node, 1 otherwise
#if defined(DSL_BY_MOD)
  return (id % DSLPERNODE) != 0;
#elif defined(HEX_ASSIGNEMENT)
  return (buf[id/(8 * sizeof(unsigned short))]>>(id%(8 * sizeof(unsigned short))))&0x01;
#else
  return !dsl_node[id];
#endif
}

int
is_dsl_core(int id)
{
  return !is_app_core(id);
}


void tm_init() {
  PF_MSG(9, "receiving");
  PF_MSG(10, "sending");

    sys_tm_init();
    if (!is_app_core(ID)) {
        //dsl node
        dsl_init();
    }
    else { //app node
        ps_init_();
        stm_tx_node = tx_metadata_node_new();
        stm_tx = tx_metadata_new();
        if (stm_tx == NULL || stm_tx_node == NULL) {
            PRINTD("Could not alloc tx metadata @ TM_INIT");
            EXIT(-1);
        }

#ifdef PGAS
        PGAS_alloc_init(0);
#endif
    }
}

void
init_system(int* argc, char** argv[])
{
  /* calculate the getticks correction if DO_TIMINGS is set */
  PF_CORRECTION;

  /* call platform level initializer */
  sys_init_system(argc, argv);

  /* initialize globals */
  ID            = NODE_ID();
  NUM_UES       = TOTAL_NODES();

  uint32_t i, tot=0;
  for (i =0; i < NUM_UES; i++) {
    if (!is_app_core(i)) {
      tot++;
    }
  }
  NUM_DSL_NODES = tot;
  NUM_APP_NODES = NUM_UES - tot;

  //	fprintf(stderr, "ID: %u\nNUM_UES: %u\nNUM_DSL_NODES: %u\nNUM_APP_NODES: %u\n",
  //		ID, NUM_UES, NUM_DSL_NODES, NUM_APP_NODES);

  init_barrier();
}
/*
 * Trampolining code for terminating everything
 */
void
tm_term()
{
  if (!is_app_core(ID)) {
    // DSL node
    // common stuff
    uint32_t c;
    for (c = 0; c < TOTAL_NODES(); c++)
      {
	if (NODE_ID() == c)
	  {
#ifdef DO_TIMINGS
	    printf("(( %02d ))", c);
#endif
	    PF_PRINT;
	  }
	BARRIER_DSL;
      }

    BARRIERW;
    // platform specific stuff
    sys_dsl_term();
  }
  else { 
    //app node
    // common stuff
    BARRIERW;
    uint32_t c;
    for (c = 0; c < TOTAL_NODES(); c++)
      {
	if (NODE_ID() == c)
	  {
#ifdef DO_TIMINGS
	    printf("(( %02d ))", c);
#endif
	    PF_PRINT;
	  }
	BARRIER;
      }
    // plaftom specific stuff
    sys_ps_term();
  }
}

void handle_abort(stm_tx_t *stm_tx, CONFLICT_TYPE reason) {
    ps_finish_all(reason);
    stm_tx->aborts++;

    switch (reason) {
        case READ_AFTER_WRITE:
            stm_tx->aborts_raw++;
            break;
        case WRITE_AFTER_READ:
            stm_tx->aborts_war++;
            break;
        case WRITE_AFTER_WRITE:
            stm_tx->aborts_waw++;
    		break;
    	default:
    		/* nothing */
    		break;
    }
    //PRINTD("  | read/write_set_free");
#ifdef PGAS
    write_set_pgas_empty(stm_tx->write_set);
#else
    write_set_empty(stm_tx->write_set);
#endif
    mem_info_on_abort(stm_tx->mem_info);
    
#ifdef BACKOFF_RETRY
    /*BACKOFF and RETRY*/
    if (BACKOFF_MAX > 0)  {
      unsigned int wait_max = pow(2, (stm_tx->retries < BACKOFF_MAX ? stm_tx->retries : BACKOFF_MAX)) * BACKOFF_DELAY;
      unsigned int wait = rand_range(wait_max);
      //PRINT("\t\t\t\t\t\t... backoff for %5d micros (retries: %3d | max: %d)", wait, stm_tx->retries, wait_max);
      ndelay(wait);
    }
    else {
      wait_cycles(50 * stm_tx->retries);
    }
#endif
    
}

void ps_publish_finish_all(unsigned int locked) {
    locked = (locked != 0) ? locked : stm_tx->write_set->nb_entries;
#ifdef PGAS
    write_entry_pgas_t *we_current = stm_tx->write_set->write_entries;
#else
    write_entry_t *we_current = stm_tx->write_set->write_entries;
#endif
    while (locked-- > 0) {
#ifdef PGAS
        // ps_publish_finish(we_current->address, we_current->value);
#else
        ps_publish_finish(to_addr(we_current[locked].address));
#endif
    }
}

void ps_publish_all() {
    unsigned int locked = 0;
#ifdef PGAS
    write_entry_pgas_t *write_entries = stm_tx->write_set->write_entries;
#else
    write_entry_t *write_entries = stm_tx->write_set->write_entries;
#endif
    unsigned int nb_entries = stm_tx->write_set->nb_entries;
    while (locked < nb_entries) {
        CONFLICT_TYPE conflict;
        tm_addr_t addr = to_addr(write_entries[locked].address);
#ifndef BACKOFF_RETRY
        unsigned int num_delays = 0;
        unsigned int delay = BACKOFF_DELAY; //nano
retry:
#endif
#ifdef PGAS
        if ((conflict = ps_publish(addr, write_entries[locked].value)) != NO_CONFLICT) {
#else
        if ((conflict = ps_publish(addr)) != NO_CONFLICT) {
#endif
            //ps_publish_finish_all(locked);
#ifndef BACKOFF_RETRY
            if (num_delays++ < BACKOFF_MAX) {
	      ndelay(delay);		      /* ndelay(rand_range(delay)); */
	      delay *= 2;
	      goto retry;
            }
#endif
            TX_ABORT(conflict);
        }
        locked++;
    }
}


uint32_t		/* boolean */
tx_cas(tm_addr_t addr, uint32_t oldval, uint32_t newval)
{
  uint32_t ret = 0;

  TX_START
    {
      tx_wlock(addr);
      uint32_t *addr_ui = (uint32_t *) addr;
      if (*addr_ui == oldval)
	{
	  *addr_ui = newval;
	  ret = 1;
	}
    }
  TX_COMMIT_NO_PUB_NO_STATS;
  
  return ret;    
}


