#include "common.h"
#include "RCCE.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "ssmp.h"

#ifdef PGAS
/*
 * Under PGAS we're using pgas_app allocator, to have fake allocations, that
 * is a very simplified allocator
 */
#  include "pgas_app.h"
#endif

nodeid_t MY_NODE_ID;
nodeid_t MY_TOTAL_NODES;

#if !defined(NOCM) && !defined(BACKOFF_RETRY) /* if any other CM (greedy, wholly, faircm) */
ssmp_mpb_line_t *abort_reason_mine;
ssmp_mpb_line_t **abort_reasons;
volatile ssmp_mpb_line_t *persisting_mine;
volatile ssmp_mpb_line_t **persisting;
t_vcharp *cm_abort_flags;
t_vcharp cm_abort_flag_mine;
#endif /* CM_H */

INLINED void sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    int64_t value,
                    CONFLICT_TYPE response);

void 
sys_init_system(int* argc, char** argv[])
{
  RCCE_init(argc, argv);

  MY_NODE_ID = RCCE_ue();
  MY_TOTAL_NODES = RCCE_num_ues();

  ssmp_init(RCCE_num_ues(), RCCE_ue());
}

void
term_system()
{
  ssmp_term();
}

sys_t_vcharp
sys_shmalloc(size_t size)
{
#ifdef PGAS
  return pgas_app_alloc(size);
#else
  return RCCE_shmalloc(size);
#endif
}

void
sys_shfree(sys_t_vcharp ptr)
{
#ifdef PGAS
  pgas_app_free((void*) ptr);
#else
  RCCE_shfree(ptr);
#endif
}


static int color(int id, void *aux) {
    return !(id % DSLNDPERNODES);
}

static CONFLICT_TYPE ps_response; //TODO: make it more sophisticated
static PS_COMMAND *ps_command, *ps_remote, *psc;

#ifndef PGAS
/*
 * Pointer to the minimum address we get from the iRCCE_shmalloc
 * Used for offsets, set in tm_init
 * Not used with PGAS, as there we rely on fakemem_malloc
 */
tm_addr_t shmem_start_address;
#endif

void
sys_tm_init()
{
  
#ifndef PGAS
    if (shmem_start_address == NULL) {
        char *start = (char *)RCCE_shmalloc(sizeof (char));
        if (start == NULL) {
            PRINTD("shmalloc shmem_init_start_address");
        }
        shmem_start_address = (tm_addr_t)start;
        RCCE_shfree((volatile unsigned char*) start);
    }
#endif

}

void
sys_ps_init_(void)
{

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
  abort_reason_mine = (ssmp_mpb_line_t *) ssmp_mpb_alloc(NODE_ID(), sizeof(ssmp_mpb_line_t));
  persisting_mine = (ssmp_mpb_line_t *) ssmp_mpb_alloc(NODE_ID(), sizeof(ssmp_mpb_line_t));
  mpb_write_line(persisting_mine, 0);
  cm_abort_flag_mine = virtual_lockaddress[NODE_ID()];
#endif /* CM_H */

#if defined(PGAS)
  pgas_app_init();
#endif  /* PGAS */

  BARRIERW;
}

void
sys_dsl_init(void)
{
  ps_command = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize
  ps_remote = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize
  psc = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize

  if (ps_command == NULL || ps_remote == NULL || psc == NULL) {
    PRINTD("malloc ps_command == NULL || ps_remote == NULL || psc == NULL");
  }

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
  abort_reasons = (ssmp_mpb_line_t **) malloc(TOTAL_NODES() * sizeof(ssmp_mpb_line_t *));
  persisting = (volatile ssmp_mpb_line_t **) malloc(TOTAL_NODES() * sizeof(ssmp_mpb_line_t *));
  assert(abort_reasons != NULL && persisting != NULL);
  uint32_t n;
  for (n = 0; n < TOTAL_NODES(); n++)
    {
      if (is_app_core(n)) 
	{
	  abort_reasons[n] = (ssmp_mpb_line_t *) ssmp_mpb_alloc(n, sizeof(ssmp_mpb_line_t));
	  persisting[n] = (ssmp_mpb_line_t *) ssmp_mpb_alloc(n, sizeof(ssmp_mpb_line_t));
	}
      else 
	{
	  abort_reasons[n] = NULL;
	  persisting[n] = NULL;
	}
  }
  cm_abort_flags = virtual_lockaddress;
#endif /* CM_H */

#if defined(PGAS)
  pgas_dsl_init();
#endif	/* PGAS */

  BARRIERW;
}

void
sys_dsl_term(void)
{
	// noop
}

void
sys_ps_term(void)
{
	// noop
}

// If value == NULL, we just return the address.
// Otherwise, we return the value.
INLINED void
sys_ps_command_reply(nodeid_t sender, PS_REPLY_TYPE cmd, tm_addr_t addr, int64_t value, CONFLICT_TYPE response)
{
  PS_REPLY reply;
  reply.type = cmd;
  reply.response = response;

  PRINTD("sys_ps_command_reply: src=%u target=%d", reply.nodeId, sender);
#ifdef PGAS
  reply.value = value;
#endif

  ssmp_msg_t *msg = (ssmp_msg_t *) &reply;
  ssmp_send(sender, msg);
}

typedef struct
{
  union
  {
    int64_t lli;
    int32_t i[2];
  };
} convert_t;

void
dsl_communication()
{
  nodeid_t sender, last_recv_from = 0;
  PS_COMMAND_TYPE command;

  ssmp_msg_t *msg;
  msg = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));

  ssmp_color_buf_t *cbuf;
  cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
  assert(msg != NULL && cbuf != NULL);

  ssmp_color_buf_init(cbuf, is_app_core);

  while (1)
    {

      last_recv_from = ssmp_recv_color_start(cbuf, msg, last_recv_from) + 1;
      sender = msg->sender;
    
      ps_remote = (PS_COMMAND *) msg;

#if defined(WHOLLY) || defined(FAIRCM)
      cm_metadata_core[sender].timestamp = (ticks) ps_remote->tx_metadata;
#elif defined(GREEDY)
      if (cm_metadata_core[sender].timestamp == 0)
	{
	  cm_metadata_core[sender].timestamp = getticks() - (ticks) ps_remote->tx_metadata;
	}
#endif

      switch (ps_remote->type)
	{
	case PS_SUBSCRIBE:
	  {
	    CONFLICT_TYPE conflict = try_subscribe(sender, ps_remote->address);
	    /* PF_STOP(11); */
#ifdef PGAS
	    uint64_t val;
	    if (ps_remote->num_words == 1)
	      {
		val = pgas_dsl_read32(ps_remote->address);
	      }
	    else
	      {
		val = pgas_dsl_read(ps_remote->address);
	      }

	    sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
				 (tm_addr_t) ps_remote->address, val, conflict);
#else  /* !PGAS */
	    sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, 
				 (tm_addr_t) ps_remote->address, 
				 0,
				 conflict);
#endif	/* PGAS */
	    if (conflict != NO_CONFLICT)
	      {
		ps_hashtable_delete_node(ps_hashtable, sender);
#if defined(GREEDY)
		cm_metadata_core[sender].timestamp = 0;
#endif
#ifdef PGAS
		PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
#endif	/* PGAS */
	      }

	    break;
	  }
	case PS_PUBLISH:
	  {
	    CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);
#ifdef PGAS

	    if (conflict == NO_CONFLICT) 
	      {
		write_set_pgas_insert(PGAS_write_sets[sender], ps_remote->write_value, ps_remote->address);
	      }
#endif	/* PGAS */

	    sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE, 
				 (tm_addr_t) ps_remote->address, 0, conflict);

	    if (conflict != NO_CONFLICT)
	      {
		ps_hashtable_delete_node(ps_hashtable, sender);

#if defined(GREEDY)
		cm_metadata_core[sender].timestamp = 0;
#endif
#ifdef PGAS
		PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
#endif	/* PGAS */
	      }

	    break;
	  }
#ifdef PGAS
	case PS_WRITE_INC:
	  {
	    CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);

	    if (conflict == NO_CONFLICT) 
	      {
		int64_t val = pgas_dsl_read(ps_remote->address) + ps_remote->write_value;

		/* PRINT("PS_WRITE_INC from %2d for %3d, off: %lld, old: %3lld, new: %lld", sender,  */
		/*       ps_remote->address, ps_remote->write_value, */
		/*       pgas_dsl_read(ps_remote->address), val); */

		write_set_pgas_insert(PGAS_write_sets[sender], val, ps_remote->address);
	      }

	    sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE,
				 (tm_addr_t) ps_remote->address,
				 0,
				 conflict);

	    if (conflict != NO_CONFLICT)
	      {
		ps_hashtable_delete_node(ps_hashtable, sender);
		PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
		/* PRINT("conflict on %d (aborting %d)", ps_remote->address, sender); */
	      }

	    break;
	  }
	case PS_LOAD_NONTX:
	  {
	    int64_t val;
	    if (ps_remote->num_words == 1)
	      {
		val = (int64_t) pgas_dsl_read32(ps_remote->address);
	      }
	    else
	      {
		val = pgas_dsl_read(ps_remote->address);
	      }

	    sys_ps_command_reply(sender, PS_LOAD_NONTX_RESPONSE,
				 (tm_addr_t) ps_remote->address,
				 val,
				 NO_CONFLICT);
		
	    break;
	  }
	case PS_STORE_NONTX:
	  {
	    pgas_dsl_write(ps_remote->address, ps_remote->write_value);
	    break;
	  }
#endif
	case PS_REMOVE_NODE:
	  {
#ifdef PGAS
	    if (ps_remote->response == NO_CONFLICT) 
	      {
		write_set_pgas_persist(PGAS_write_sets[sender]);
	      }
	    PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
#endif
	    ps_hashtable_delete_node(ps_hashtable, sender);

#if defined(GREEDY)
	    cm_metadata_core[sender].timestamp = 0;
#endif
	    break;
	  }
	case PS_UNSUBSCRIBE:
	  ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
	  break;
	case PS_PUBLISH_FINISH:
	  ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
	  break;
	case PS_STATS:
	  {
	    PS_STATS_CMD_T* ps_rem_stats = (PS_STATS_CMD_T*) ps_remote;

	    if (ps_rem_stats->tx_duration)
	      {
		stats_aborts += ps_rem_stats->aborts;
		stats_commits += ps_rem_stats->commits;
		stats_duration += ps_rem_stats->tx_duration;
		stats_max_retries = stats_max_retries < ps_rem_stats->max_retries ? ps_rem_stats->max_retries : stats_max_retries;
		stats_total += ps_rem_stats->commits + ps_rem_stats->aborts;
	      }
	    else
	      {
		stats_aborts_raw += ps_rem_stats->aborts_raw;
		stats_aborts_war += ps_rem_stats->aborts_war;
		stats_aborts_waw += ps_rem_stats->aborts_waw;
	      }
	    if (++stats_received >= 2*NUM_APP_NODES)
	      {
		uint32_t n;
		for (n = 0; n < TOTAL_NODES(); n++)
		  {
		    BARRIER_DSL;
		    if (n == NODE_ID())
		      {
			ssht_stats_print(ps_hashtable, SSHT_DBG_UTILIZATION_DTL);
		      }
		  }

		BARRIER_DSL;
		if (NODE_ID() == min_dsl_id()) 
		  {
		    print_global_stats();
		  }
		return;
	      }
	    break;
	  }
	default:
	  {
	    sys_ps_command_reply(sender, PS_UKNOWN_RESPONSE,
				 NULL,
				 NULL,
				 NO_CONFLICT);
	  }
	}
    }
}




/*
 * Seeding the rand()
 */
void
srand_core()
{
    double timed_ = RCCE_wtime();
    unsigned int timeprfx_ = (unsigned int) timed_;
    unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
    srand(time_ + (13 * (RCCE_ue() + 1)));
}


INLINED void
wait_cycles(uint64_t ncycles)
{
  if (ncycles < 24)
    {
      return;
    }
  else if (ncycles < 256) 
    {
      volatile int64_t _ncycles = ncycles;
      _ncycles >>= 3;
      _ncycles -= 3;
      while (_ncycles-- > 0)
	{
	  asm("nop");
	}
    }
  else
    {
      ticks _target = getticks() + ncycles - 50;
      while (getticks() < _target) ;
    }
}


void 
udelay(uint64_t micros)
{
  ticks in_cycles = REF_SPEED_GHZ * 1000 * micros;
  wait_cycles(in_cycles);
}

void 
ndelay(uint64_t nanos)
{
  ticks in_cycles = REF_SPEED_GHZ * nanos;
  wait_cycles(in_cycles);
}

void
init_barrier()
{
  ssmp_barrier_init(1, 0, is_app_core);
  ssmp_barrier_init(3, 0, is_dsl_core);

  BARRIERW;
}
