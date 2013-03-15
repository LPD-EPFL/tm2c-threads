#define _GNU_SOURCE
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <limits.h>
#include <ssmp.h>
#include "common.h"
#include "tm.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "mcore_malloc.h"

#include "hash.h"

#define ALL_REAL_CORES

/* try to distribute the work around the real cores */
uint8_t id_to_core_id[] =
  {
    0,  8, 16, 24, 32, 40, 48, 56,
    1,  9, 17, 25, 33, 41, 49, 57,
    2, 10, 18, 26, 34, 42, 50, 58,
    3, 11, 19, 27, 35, 43, 51, 59,
    4, 12, 20, 28, 36, 44, 52, 60,
    5, 13, 21, 29, 37, 45, 53, 61,
    6, 14, 22, 30, 38, 46, 54, 62,
    7, 15, 23, 31, 39, 47, 55, 63,
  };


/* try to place the service on separate cores  -- 1 server / 4*/
/* uint8_t id_to_core_id[] = */
/*   { */
/*     0,  8, 16, 1, 32, 40, 2, 56, */
/*     24,  3, 17, 25, 4, 41, 49, 57, */
/*     48, 10, 18, 26, 34, 7, 50, 5, */
/*     9, 11, 6, 27, 35, 43, 51, 59, */
/*     33, 12, 20, 28, 36, 44, 52, 60, */
/*     58, 13, 21, 29, 37, 45, 53, 61, */
/*     19, 14, 22, 30, 38, 46, 54, 62, */
/*     42, 15, 23, 31, 39, 47, 55, 63, */
/*   }; */

/* id === core */
/* uint8_t id_to_core_id[] = */
/*   { */
/*     0, 1, 2, 3, 4, 5, 6, 7, */
/*     8, 9, 10, 11, 12, 13, 14, 15, */
/*     16, 17, 18, 19, 20, 21, 22, 23, */
/*     24, 25, 26, 27, 28, 29, 30, 31, */
/*     32, 33, 34, 35, 36, 37, 38, 39, */
/*     40, 41, 42, 43, 44, 45, 46, 47, */
/*     48, 49, 50, 51, 52, 53, 54, 55, */
/*     56, 57, 58, 59, 60, 61, 62, 63 */
/*   }; */


PS_REPLY* ps_remote_msg; // holds the received msg

INLINED nodeid_t min_dsl_id();

INLINED void sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    int64_t value,
                    CONFLICT_TYPE response);
/*
 * For cluster conf, we need a different kind of command line parameters.
 * Since there is no way for a node to know it's identity, we need to pass it,
 * along with the total number of nodes.
 * To make sure we don't rely on any particular order, params should be passed
 * as: -id=ID -total=TOTAL_NODES
 */
nodeid_t MY_NODE_ID;
nodeid_t MY_TOTAL_NODES;


#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
int32_t **cm_abort_flags;
int32_t *cm_abort_flag_mine;
#if defined(GREEDY) && defined(GREEDY_GLOBAL_TS)
ticks* greedy_global_ts;
#endif
#endif /* NOCM */


void
sys_init_system(int* argc, char** argv[])
{

	if (*argc < 2) {
		fprintf(stderr, "Not enough parameters (%d)\n", *argc);
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s -total=TOTAL_NODES ...\n", (*argv)[0]);
		EXIT(1);
	}

	int p = 1;
	int found = 0;
	while (p < *argc) {
		if (strncmp("-total=", (*argv)[p], strlen("-total=")) == 0) {

			char *cf = (*argv)[p] + strlen("-total=");
			MY_TOTAL_NODES = atoi(cf);
			(*argv)[p] = NULL;
			found = 1;
		}
		p++;
	}
	if (!found) {
		fprintf(stderr, "Did not pass all parameters\n");
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s -total=TOTAL_NODES ...\n", (*argv)[0]);
		EXIT(1);
	}
	p = 1;
	int cur = 1;
	while (p < *argc) {
		if ((*argv)[p] == NULL) {
			p++;
			continue;
		}
		(*argv)[cur] = (*argv)[p];
		cur++;
		p++;
	}
	*argc = *argc - (p-cur);

	MY_NODE_ID = 0;

	ssmp_init(MY_TOTAL_NODES);

	nodeid_t rank;
	for (rank = 1; rank < MY_TOTAL_NODES; rank++) {
		PRINTD("Forking child %u", rank);
		pid_t child = fork();
		if (child < 0) {
			PRINT("Failure in fork():\n%s", strerror(errno));
		} else if (child == 0) {
			goto fork_done;
		}
	}
	rank = 0;
fork_done:
	PRINTD("Initializing child %u", rank);
	MY_NODE_ID = rank;
	ssmp_mem_init(MY_NODE_ID, MY_TOTAL_NODES);
	// Now, pin the process to the right core (NODE_ID == core id)
#if defined(ALL_REAL_CORES)
	set_cpu(id_to_core_id[rank]);
#else
	set_cpu(rank);
#endif
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
  return MCORE_shmalloc(size);
#endif
}

void
sys_shfree(sys_t_vcharp ptr)
{
#ifdef PGAS
  pgas_app_free((void*) ptr);
#else
  MCORE_shfree(ptr);
#endif
}

void
sys_tm_init()
{

}

void
sys_ps_init_(void)
{
#if defined(PGAS)
  pgas_app_init();
#else  /* PGAS */
  APP_EXEC_ORDER
    {
      MCORE_shmalloc_init(1024*1024*1024); //1GB
    }
  APP_EXEC_ORDER_END;
#endif /* PGAS */

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
  cm_abort_flag_mine = cm_init(NODE_ID());
  *cm_abort_flag_mine = NO_CONFLICT;

#if defined(GREEDY) && defined(GREEDY_GLOBAL_TS)
  greedy_global_ts = cm_greedy_global_ts_init();
#endif

#endif

  BARRIERW;

  ps_remote_msg = NULL;
  PRINTD("sys_ps_init: done");

  BARRIERW;
}

void
sys_dsl_init(void)
{

#if defined(PGAS)
  pgas_dsl_init();
#else  /* PGAS */
  uint32_t c;
  for (c = 0; c < TOTAL_NODES(); c++)
    {
      if (NODE_ID() == c)
	{
	  MCORE_shmalloc_init(1024 * 1024 * 1024);
	}
      BARRIER_DSL;
    }
#endif	/* PGAS */

  BARRIERW;

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
  cm_abort_flags = (int32_t **) malloc(TOTAL_NODES() * sizeof(int32_t *));
  assert(cm_abort_flags != NULL);

  uint32_t i;
  for (i = 0; i < TOTAL_NODES(); i++) 
    {
      //TODO: make it open only for app nodes
      if (is_app_core(i))
	{
	  cm_abort_flags[i] = cm_init(i);    
	}
    }
#endif
  BARRIERW;
}

void
sys_dsl_term(void)
{
  BARRIERW;
}

void
sys_ps_term(void)
{
  BARRIERW;
}


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

  /* PF_STOP(11); */
  /* PF_START(10); */
  ssmp_send(sender, msg);
  /* PF_STOP(10); */
}


void
dsl_communication()
{
  nodeid_t sender;

  ssmp_msg_t *msg;
  ssmp_color_buf_t *cbuf;
  static PS_COMMAND *ps_remote;

  msg = (ssmp_msg_t*) memalign(CACHE_LINE_SIZE, sizeof(ssmp_msg_t));
  cbuf = (ssmp_color_buf_t*) memalign(CACHE_LINE_SIZE, sizeof(ssmp_color_buf_t));
  ps_remote = (PS_COMMAND*) memalign(CACHE_LINE_SIZE, sizeof(PS_COMMAND));
  assert(msg != NULL && cbuf != NULL && ps_remote != 0);
  assert((uintptr_t) msg % CACHE_LINE_SIZE == 0);
  assert((uintptr_t) cbuf % CACHE_LINE_SIZE == 0);
  assert((uintptr_t) ps_remote % CACHE_LINE_SIZE == 0);

  PF_MSG(11, "servicing");


  ssmp_color_buf_init(cbuf, is_app_core);

  while(1) 
    {
      /* PF_START(9); */
      ssmp_recv_color_start(cbuf, msg);
      /* PF_STOP(9); */
      PF_START(11);
      sender = msg->sender;

      ps_remote = (PS_COMMAND *) msg;
 
      /* PRINT(" XXX cmd %2d from %d for %lu", msg->w0, sender, ps_remote->address); */

#if defined(WHOLLY) || defined(FAIRCM)
      cm_metadata_core[sender].timestamp = (ticks) ps_remote->tx_metadata;
#elif defined(GREEDY)
      if (cm_metadata_core[sender].timestamp == 0)
	{
#  ifdef GREEDY_GLOBAL_TS
	  cm_metadata_core[sender].timestamp = (ticks) ps_remote->tx_metadata;
#  else
	  cm_metadata_core[sender].timestamp = getticks() - (ticks) ps_remote->tx_metadata;
#  endif
	}
#endif
    

      switch (ps_remote->type) 
	{
	case PS_SUBSCRIBE:
	  {
	    CONFLICT_TYPE conflict = try_subscribe(sender, ps_remote->address);
	    PF_STOP(11);
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
	  /*     case PS_CAS: */
	  /*       { */
	  /* 	CONFLICT_TYPE conflict = ssht_insert_w_test(ps_hashtable, sender, ps_remote->address); */

	  /* 	if (conflict == NO_CONFLICT) */
	  /* 	  { */
	  /* 	    uint32_t *addr = (uint32_t *) ps_remote->address; */
	  /* 	    if (*addr == ps_remote->oldval) */
	  /* 	      { */
	  /* 		*addr = ps_remote->write_value; */
	  /* 		conflict = CAS_SUCCESS; */
	  /* 	      } */
	  /* 	  } */

	  /* 	sys_ps_command_reply(sender, PS_CAS_RESPONSE, */
	  /* 			     (tm_addr_t) ps_remote->address, */
	  /* 			     NULL, */
	  /* 			     conflict); */

	  /* 	break; */
	  /*       } */
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
	    PS_STATS_CMD_SEND_T* ps_rem_stats = (PS_STATS_CMD_SEND_T*) ps_remote;
	    switch(ps_rem_stats->stats_type)
	      {
	      case 0:
		stats_duration += ps_rem_stats->tx_duration;
		break;
	      case 1:
		stats_aborts += ps_rem_stats->aborts;
		break;
	      case 2:
		stats_commits += ps_rem_stats->commits;
		break;
	      case 3:
		stats_max_retries = stats_max_retries < ps_rem_stats->max_retries 
		  ? ps_rem_stats->max_retries : stats_max_retries;
		break;
	      case 4:
		stats_aborts_raw += ps_rem_stats->aborts_raw;
		break;
	      case 5:
		stats_aborts_war += ps_rem_stats->aborts_war;
		break;
	      case 6:
		stats_aborts_waw += ps_rem_stats->aborts_waw;
		break;
	      default:
		break;
	      }

	    if (++stats_received >= 7*NUM_APP_NODES) 
	      {
		stats_total = stats_commits + stats_aborts;
		uint32_t n;
		for (n = 0; n < TOTAL_NODES(); n++)
		  {
		    BARRIER_DSL;
		    if (n == NODE_ID())
		      {
			ssht_stats_print(ps_hashtable, 0);
		      }
		    BARRIER_DSL;
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
	    /* PF_START(1); */
	    sys_ps_command_reply(sender, PS_UKNOWN_RESPONSE,
				 NULL,
				 0,
				 NO_CONFLICT);
	    /* PF_STOP(1); */
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
	double timed_ = wtime();
	unsigned int timeprfx_ = (unsigned int) timed_;
	unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
	srand(time_ + (13 * (ID + 1)));
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
  ssmp_barrier_init(14, 0, is_dsl_core);

  BARRIERW;
}

void
app_barrier()
{

}

void
global_barrier()
{

}

#if !defined(NOCM)	/* if any other CM (greedy, wholly, faircm) */
static int32_t *
cm_init(nodeid_t node) {
   char keyF[50];
   sprintf(keyF,"/cm_abort_flag%03d", node);

   size_t cache_line = 64;

   int abrtfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
   if (abrtfd<0)
   {
      if (errno != EEXIST)
      {
         perror("In shm_open");
         exit(1);
      }

      //this time it is ok if it already exists                                                    
      abrtfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (abrtfd<0)
      {
         perror("In shm_open");
         exit(1);
      }
   }
   else
   {
      //only if it is just created                                                                 
     if(ftruncate(abrtfd, cache_line))
       {
	 printf("ftruncate");
       }
   }

   int32_t *tmp = (int32_t *) mmap(NULL, 64, PROT_READ | PROT_WRITE, MAP_SHARED, abrtfd, 0);
   assert(tmp != NULL);
   
   return tmp;
}

#if defined(GREEDY) && defined(GREEDY_GLOBAL_TS)
static ticks* 
cm_greedy_global_ts_init()
{
   char keyF[50];
   sprintf(keyF,"/cm_greedy_global_ts");

   size_t cache_line = 64;

   int abrtfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
   if (abrtfd<0)
   {
      if (errno != EEXIST)
      {
         perror("In shm_open");
         exit(1);
      }

      //this time it is ok if it already exists                                                    
      abrtfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (abrtfd<0)
      {
         perror("In shm_open");
         exit(1);
      }
   }
   else
   {
      //only if it is just created                                                                 
     if(ftruncate(abrtfd, cache_line))
       {
	 printf("ftruncate");
       }
   }

   ticks* tmp = (ticks*) mmap(NULL, 64, PROT_READ | PROT_WRITE, MAP_SHARED, abrtfd, 0);
   assert(tmp != NULL);
   
   return tmp;
}

inline ticks
greedy_get_global_ts()
{
  return __sync_add_and_fetch (greedy_global_ts, 1);
}
#endif

#endif	/* NOCM */