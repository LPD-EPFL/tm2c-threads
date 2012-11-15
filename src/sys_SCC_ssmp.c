#include "common.h"
#include "RCCE.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "ssmp.h"

#ifdef PGAS
/*
 * Under PGAS we're using fakemem allocator, to have fake allocations, that
 * mimic those of RCCE_shmalloc
 */
#endif

nodeid_t MY_NODE_ID;
nodeid_t MY_TOTAL_NODES;

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
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
                    uint32_t* value,
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
	return fakemem_malloc(size);
#else
	return RCCE_shmalloc(size);
#endif
}

void
sys_shfree(sys_t_vcharp ptr)
{
#ifdef PGAS
	fakemem_free((void*)ptr);
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

  BARRIERW
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

  BARRIERW
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
sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response)
{
  PS_REPLY reply;
  reply.type = command;
  reply.response = response;

  PRINTD("sys_ps_command_reply: src=%u target=%d", reply.nodeId, sender);
#ifdef PGAS
  if (value != NULL) {
    reply.value = *value;
    PRINTD("sys_ps_command_reply: read value %u\n", reply.value);
  } else {
    reply.address = (uintptr_t) address;
  }
#else
  reply.address = (uintptr_t) address;
#endif

  ssmp_sendl(sender, (ssmp_msg_t *) &reply, 6);
}


//#define DEBUG_UTILIZATION
#ifdef DEBUG_UTILIZATION
uint64_t write_reqs_num = 0, read_reqs_num = 0;
#endif

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

  while (1) {

    last_recv_from = ssmp_recv_color_start(cbuf, msg, last_recv_from, 6) + 1;
    sender = msg->sender;
    
    ps_remote = (PS_COMMAND *) msg;


#if defined(WHOLLY) || defined(FAIRCM)
    cm_metadata_core[sender].timestamp = (ticks) ps_remote->tx_metadata;
#elif defined(GREEDY)
    if (cm_metadata_core[sender].timestamp == 0) {
      cm_metadata_core[sender].timestamp = getticks() - (ticks) ps_remote->tx_metadata;
    }
#endif



    switch (ps_remote->type) {
    case PS_SUBSCRIBE:
#ifdef DEBUG_UTILIZATION
      read_reqs_num++;
#endif

#ifdef PGAS
      sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
			   ps_remote->address, 
			   PGAS_read(ps_remote->address),
			   try_subscribe(sender, ps_remote->address));
#else
      sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, 
			   (tm_addr_t) ps_remote->address, 
			   NULL,
			   try_subscribe(sender, ps_remote->address));
      //sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, address, NO_CONFLICT);
#endif
      break;
    case PS_PUBLISH:
      {

#ifdef DEBUG_UTILIZATION
	write_reqs_num++;
#endif

	CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);
#ifdef PGAS
	if (conflict == NO_CONFLICT) {
	  write_set_pgas_insert(PGAS_write_sets[sender], ps_remote->write_value, ps_remote->address);
	}
#endif
	sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE, 
			     (tm_addr_t) ps_remote->address,
			     NULL,
			     conflict);
	break;
      }
#ifdef PGAS
    case PS_WRITE_INC:
      {

#ifdef DEBUG_UTILIZATION
	write_reqs_num++;
#endif
	CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);
	if (conflict == NO_CONFLICT) {
	  write_set_pgas_insert(PGAS_write_sets[sender], 
				*(int *) PGAS_read(ps_remote->address) + ps_remote->write_value,
                                ps_remote->address);
	}
	sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE,
			     ps_remote->address,
			     NULL,
			     conflict);
	break;
      }
    case PS_LOAD_NONTX:
      {
	sys_ps_command_reply(sender, PS_LOAD_NONTX_RESPONSE,
			     ps_remote->address,
			     PGAS_read(ps_remote->address),
			     NO_CONFLICT);
		
	break;
      }
    case PS_STORE_NONTX:
      {
	PGAS_write(ps_remote->address, (int) ps_remote->write_value);
	break;
      }
#endif
    case PS_REMOVE_NODE:
#ifdef PGAS
      if (ps_remote->response == NO_CONFLICT) {
	write_set_pgas_persist(PGAS_write_sets[sender]);
      }
      PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
#endif
      ps_hashtable_delete_node(ps_hashtable, sender);
      break;
    case PS_UNSUBSCRIBE:
      ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
      break;
    case PS_PUBLISH_FINISH:
      ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
      break;
    case PS_STATS:
      {
	if (ps_remote->tx_duration) {
	  stats_aborts += ps_remote->aborts;
	  stats_commits += ps_remote->commits;
	  stats_duration += ps_remote->tx_duration;
	  stats_max_retries = stats_max_retries < ps_remote->max_retries ? ps_remote->max_retries : stats_max_retries;
	  stats_total += ps_remote->commits + ps_remote->aborts;
	}
	else {
	  stats_aborts_raw += ps_remote->aborts_raw;
	  stats_aborts_war += ps_remote->aborts_war;
	  stats_aborts_waw += ps_remote->aborts_waw;
	}
	if (++stats_received >= 2*NUM_APP_NODES) {
	  ONCE {
	    print_global_stats();

	    print_hashtable_usage();

	  }

#ifdef DEBUG_UTILIZATION
	  PRINT("*** Completed requests: %llu", (long long unsigned int) read_reqs_num + write_reqs_num);
#endif

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
  if (ncycles < 256) 
    {
      volatile int64_t _ncycles = ncycles;
      _ncycles >>= 3;
      _ncycles -= 3;
      while (_ncycles-- > 0)
	{
	  asm("nop");
	}
    }
  else {
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

  BARRIERW
}
