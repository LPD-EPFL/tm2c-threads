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
#include "fakemem.h"
#endif

INLINED void sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response);


void 
sys_init_system(int* argc, char** argv[])
{
  RCCE_init(argc, argv);
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

  ssmp_sendl(sender, (ssmp_msg_t *) &reply, sizeof(PS_REPLY)/sizeof(int));
}


void
dsl_communication()
{
  int sender;
  ssmp_msg_t msg;
  PS_COMMAND_TYPE command;

  ssmp_color_buf_t cbuf;
  ssmp_color_buf_init(&cbuf, color_app);

  while (1) {

    //    ssmp_recv_color(&cbuf, &msg, sizeof(*ps_remote)/sizeof(int));
    ssmp_recv_color(&cbuf, &msg, 6);
    sender = msg.sender;
    
    ps_remote = (PS_COMMAND *) &msg;

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
			   ps_remote->address, 
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
	  write_set_pgas_insert(PGAS_write_sets[sender],
				ps_remote->write_value, 
				ps_remote->address);
	}
#endif
	sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE, 
			     ps_remote->address,
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
	  if (NODE_ID() == 0) {
	    print_global_stats();

	    print_hashtable_usage();

	  }

#ifdef DEBUG_UTILIZATION
	  PRINT("*** Completed requests: %d", read_reqs_num + write_reqs_num);
#endif

	  EXIT(0);
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


void 
udelay(unsigned int micros)
{
    double __ts_end = RCCE_wtime() + ((double) micros / 1000000);
    while (RCCE_wtime() < __ts_end);
}

void
init_barrier()
{
  ssmp_barrier_wait(0);
}
