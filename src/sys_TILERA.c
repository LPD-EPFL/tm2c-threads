#include "common.h"
#include "pubSubTM.h"
#include "dslock.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>


#ifdef PGAS
/*
 * Under PGAS we're using fakemem allocator, to have fake allocations, that
 * mimic those of RCCE_shmalloc
 */
#include "fakemem.h"
#endif

#if !defined(NOCM) && !defined(BACKOFF_RETRY) 			/* if any other CM (greedy, wholly, faircm) */
int32_t **cm_abort_flags;
int32_t *cm_abort_flag_mine;
#endif /* NOCM */


#define DEBUG_UTILIZATION_OFF
#ifdef  DEBUG_UTILIZATION
unsigned int read_reqs_num = 0, write_reqs_num = 0;
#endif

#define TM_MEM_SIZE      (128 * 1024 * 1024)

static PS_COMMAND* ps_remote;
static PS_REPLY* ps_reply;
DynamicHeader* udn_header; //headers for messaging
/* uint32_t* demux_tags; */
/* uint32_t demux_tag_mine; */
tmc_sync_barrier_t *barrier_apps, *barrier_all, *barrier_dsl; //BARRIERS

void
sys_init_system(int* argc, char** argv[]) {
  char *executable_name = (*argv)[0];
  NUM_UES = atoi(*(++(*argv)));

  (*argv)[0] = executable_name;
  (*argc)--;


#ifndef PGAS    /*DO NOT allocate the shared memory if you have PGAS mem model*/
    // Allocate an array of integers.  We use tmc_alloc_set_shared() to allocate
    // memory that will be shared in all child processes.  This mechanism is
    // sufficient if an application can allocate all of its shared memory
    // in the parent.  If an application needs to dynamically allocate
    // shared memory in child processes, use the tmc_cmem APIs.
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_shared(&alloc);
#ifdef DISABLE_CC
    tmc_alloc_set_home(&alloc, MAP_CACHE_NO_LOCAL);
#else
    tmc_alloc_set_home(&alloc, TMC_ALLOC_HOME_HASH);
#endif
    uint32_t* data = tmc_alloc_map(&alloc, TM_MEM_SIZE);
    if (data == NULL)
        tmc_task_die("Failed to allocate memory.");

    MCORE_shmalloc_set((void*) data);
#endif

    //initialize shared memory
    tmc_cmem_init(0);

    cpu_set_t cpus;
    if (tmc_cpus_get_my_affinity(&cpus) != 0)
        tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");

    // Reserve the UDN rectangle that surrounds our cpus.
    if (tmc_udn_init(&cpus) < 0)
        tmc_task_die("Failure in 'tmc_udn_init(0)'.");
    uint32_t i, tot = 0;
    for (i = 0; i < NUM_UES; i++) {
      if (!is_app_core(i)) {
	tot++;
      }
    }
    NUM_DSL_NODES = tot;
    NUM_APP_NODES = NUM_UES - tot;

    barrier_apps = (tmc_sync_barrier_t *) tmc_cmem_calloc(1, sizeof (tmc_sync_barrier_t));
    barrier_all = (tmc_sync_barrier_t *) tmc_cmem_calloc(1, sizeof (tmc_sync_barrier_t));
    barrier_dsl = (tmc_sync_barrier_t *) tmc_cmem_calloc(1, sizeof (tmc_sync_barrier_t));
    if (barrier_all == NULL || barrier_apps == NULL || barrier_dsl == NULL) {
        tmc_task_die("Failure in allocating mem for barriers");
    }
    tmc_sync_barrier_init(barrier_all, NUM_UES);
    tmc_sync_barrier_init(barrier_apps, NUM_APP_NODES);
    tmc_sync_barrier_init(barrier_dsl, NUM_DSL_NODES);

    if (tmc_cpus_get_my_affinity(&cpus) != 0)
      {
        tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");
      }
    if (tmc_cpus_count(&cpus) < NUM_UES)
      {
        tmc_task_die("Insufficient cpus (%d < %d).", tmc_cpus_count(&cpus), NUM_UES);
      }

    int watch_forked_children = tmc_task_watch_forked_children(1);

    ID = 0;
    PRINTD("will create %d more procs", NUM_UES - 1);

    int rank;
    for (rank = 1; rank < NUM_UES; rank++) {
        pid_t child = fork();
        if (child < 0)
            tmc_task_die("Failure in 'fork()'.");
        if (child == 0)
            goto done;
    }
    rank = 0;

    (void) tmc_task_watch_forked_children(watch_forked_children);

done:

    ID = rank;

    if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, rank)) < 0)
        tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");

    if (rank != tmc_cpus_get_my_cpu()) {
        PRINT("******* i am not CPU %d", tmc_cpus_get_my_cpu());
    }

    // Now that we're bound to a core, attach to our UDN rectangle.
    if (tmc_udn_activate() < 0)
        tmc_task_die("Failure in 'tmc_udn_activate()'.");

    udn_header = (DynamicHeader *) malloc(NUM_UES * sizeof (DynamicHeader));

    if (udn_header == NULL) {
        tmc_task_die("Failure in allocating dynamic headers");
    }

    int r;
    for (r = 0; r < NUM_UES; r++) {
        int _cpu = tmc_cpus_find_nth_cpu(&cpus, r);
        DynamicHeader header = tmc_udn_header_from_cpu(_cpu);
        udn_header[r] = header;
    }

}

void
term_system() {
    // noop
}

void init_barrier() {
    //noop
}


sys_t_vcharp
sys_shmalloc(size_t size) {
#ifdef PGAS
    return fakemem_malloc(size);
#else
    return (sys_t_vcharp) MCORE_shmalloc(size);
#endif
}

void
sys_shfree(sys_t_vcharp ptr) {
#ifdef PGAS
    fakemem_free((void*) ptr);
#else
    MCORE_shfree(ptr);
#endif
}

static CONFLICT_TYPE ps_response;
static PS_COMMAND *psc;

void
sys_tm_init() 
{
}

void
sys_ps_init_(void)
{

#if !defined(NOCM) && !defined(BACKOFF_RETRY) 			/* if any other CM (greedy, wholly, faircm) */
  cm_abort_flag_mine = cm_init(NODE_ID());
  *cm_abort_flag_mine = NO_CONFLICT;
#endif
  BARRIERW;

  /* demux_tags = (uint32_t*) malloc(TOTAL_NODES() * sizeof(uint32_t)); */
  /* assert(demux_tags != NULL); */

  /* nodeid_t n; */
  /* for (n = 0; n < TOTAL_NODES(); n++) */
  /*   { */
  /* if (is_dsl_core(n)) */
  /* 	{ */
  /* 	  uint32_t id_seq = dsl_id_seq(n); */

  /* switch (id_seq % 4) */
  /*   { */
  /*   case 0: */
  /*     demux_tags[n] = UDN0_DEMUX_TAG; */
  /*     break; */
  /*   case 1: */
  /*     demux_tags[n] = UDN1_DEMUX_TAG; */
  /*     break; */
  /*   case 2: */
  /*     demux_tags[n] = UDN2_DEMUX_TAG; */
  /*     break; */
  /*   default: 			/\* 3 *\/ */
  /*     demux_tags[n] = UDN3_DEMUX_TAG; */
  /*     break; */
  /*   } */
  /* } */
  /* } */

  BARRIERW;
}

void
sys_dsl_init(void)
{
  ps_remote = (PS_COMMAND*) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
  psc = (PS_COMMAND*) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
  ps_reply = (PS_REPLY*) malloc(sizeof(PS_REPLY));
  assert(ps_remote != NULL && psc != NULL && ps_reply != NULL);

  nodeid_t id_seq = dsl_id_seq(NODE_ID());
  /* switch (id_seq % 4) */
  /*   { */
  /*   case 0: */
  /*     demux_tag_mine = UDN0_DEMUX_TAG; */
  /*     break; */
  /*   case 1: */
  /*     demux_tag_mine = UDN1_DEMUX_TAG; */
  /*     break; */
  /*   case 2: */
  /*     demux_tag_mine = UDN2_DEMUX_TAG; */
  /*     break; */
  /*   default: 			/\* 3 *\/ */
  /*     demux_tag_mine = UDN3_DEMUX_TAG; */
  /*     break; */
  /*   } */

#if !defined(NOCM) && !defined(BACKOFF_RETRY) /* if any other CM (greedy, wholly, faircm) */
  cm_abort_flags = (int32_t **) malloc(TOTAL_NODES() * sizeof(int32_t *));
  assert(cm_abort_flags != NULL);

  BARRIERW;

  uint32_t i;
  for (i = 0; i < TOTAL_NODES(); i++) 
    {
      //TODO: make it open only for app nodes
      if (is_app_core(i))
	{
	  cm_abort_flags[i] = cm_init(i);    
	}
    }
#else
  BARRIERW;
#endif

  BARRIERW;
}

  void
    sys_dsl_term(void) {
    // noop
  }

  void
    sys_ps_term(void) {
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
  ps_reply->type = command;
  ps_reply->response = response;

  PRINTD("sys_ps_command_reply: src=%u target=%d", ps_reply->nodeId, sender);
#ifdef PGAS
  if (value != NULL) 
    {
      reply->value = *value;
      PRINTD("sys_ps_command_reply: read value %u\n", ps_reply->value);
    } 
  tmc_udn_send_buffer(udn_header[sender], UDN0_DEMUX_TAG, ps_reply, PS_REPLY_SIZE_WORDS);
#else
  tmc_udn_send_buffer(udn_header[sender], UDN0_DEMUX_TAG, ps_reply, PS_REPLY_SIZE_WORDS);
/*   tmc_udn_send_1(udn_header[sender], UDN0_DEMUX_TAG, ps_reply->to_word); */
#endif	/* PGAS */

  /* tmc_udn_send_buffer(udn_header[sender], demux_tag_mine, &reply, PS_REPLY_WORDS); */
}


void 
dsl_communication()
{
  nodeid_t sender;
  /* uint32_t* cmd = (uint32_t*) ps_remote; */

  PF_MSG(5, "servicing a request");
  while (1) 
    {
      /* PF_START(5); */
      tmc_udn0_receive_buffer(ps_remote, PS_COMMAND_SIZE_WORDS);

      sender = ps_remote->nodeId;
      
#if defined(WHOLLY) || defined(FAIRCM)
      cm_metadata_core[sender].timestamp = (ticks) ps_remote->tx_metadata;
#elif defined(GREEDY)
      if (cm_metadata_core[sender].timestamp == 0)
	{
	  cm_metadata_core[sender].timestamp = getticks() - (ticks) ps_remote->tx_metadata;
	}
#endif
      /* PRINT("CMD from %02d | type: %d | addr: %u", sender, ps_remote->type, ps_remote->address); */

      switch (ps_remote->type) 
	{
	case PS_SUBSCRIBE:
	  {

	    CONFLICT_TYPE conflict = try_subscribe(sender, ps_remote->address);
#ifdef PGAS
	    /*
	      PRINT("RL addr: %3d, val: %d", address, PGAS_read(address));
	    */
	    sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
				 ps_remote->address, 
				 PGAS_read(ps_remote->address),
				 try_subscribe(sender, ps_remote->address));
#else
	    sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, 
				 (tm_addr_t) ps_remote->address, 
				 NULL,
				 conflict);
	    //sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, address, NO_CONFLICT);
#endif
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
		write_set_pgas_insert(PGAS_write_sets[sender],
				      ps_remote->write_value, 
				      ps_remote->address);
	      }
#endif
	    sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE, 
				 (tm_addr_t) ps_remote->address,
				 NULL,
				 conflict);

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

#ifdef DEBUG_UTILIZATION
	    write_reqs_num++;
#endif
	    CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);
	    if (conflict == NO_CONFLICT) {
	      //		      PRINT("wval for %d is %d", address, write_value);
	      /*
		PRINT("PS_WRITE_INC from %2d for %3d, old: %3d, new: %d", sender, address, PGAS_read(address),
		PGAS_read(address) + write_value);
	      */
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
	    //		PRINT("((non-tx ld: from %d, addr %d (val: %d)))", sender, address, (*PGAS_read(address)));
	    sys_ps_command_reply(sender, PS_LOAD_NONTX_RESPONSE,
				 ps_remote->address,
				 PGAS_read(ps_remote->address),
				 NO_CONFLICT);
		
	    break;
	  }
	case PS_STORE_NONTX:
	  {
	    //		PRINT("((non-tx st: from %d, addr %d (val: %d)))", sender, address, (write_value));
	    PGAS_write(ps_remote->address, (int) ps_remote->write_value);
	    break;
	  }
#endif
	case PS_REMOVE_NODE:
	  {
#ifdef PGAS
	    if (ps_remote->response == NO_CONFLICT) {
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
	    uint32_t w;
	    int left = PS_STATS_CMD_SIZE_WORDS - PS_COMMAND_SIZE_WORDS;
#if defined(__tilepro__)
	    uint32_t collect[PS_STATS_CMD_SIZE_WORDS - PS_COMMAND_SIZE_WORDS];
#else  /* __tilegx__ */
	    uint64_t collect[PS_STATS_CMD_SIZE_WORDS - PS_COMMAND_SIZE_WORDS];
#endif	/* __tilepro__ */

	    for (w = 0; w < left; w++)
	      {
		collect[w] = tmc_udn0_receive();
	      }

	    PS_STATS_CMD_T ps_stats;
	    memcpy(&ps_stats, ps_remote, PS_COMMAND_SIZE);
	    if (left > 0)
	      {
		void* ps_stats_mid = ((void*) &ps_stats) + PS_COMMAND_SIZE;
		memcpy(ps_stats_mid, collect, PS_STATS_CMD_SIZE - PS_COMMAND_SIZE);
	      }
	    
	    PS_STATS_CMD_T* ps_rem_stats = &ps_stats;

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
#if defined(USE_HASHTABLE_SSHT)
			ssht_stats_print(ps_hashtable, SSHT_DBG_UTILIZATION_DTL);
#endif
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
	    sys_ps_command_reply(sender, PS_UKNOWN_RESPONSE,
				 NULL,
				 NULL,
				 NO_CONFLICT);
	  }
	}

      /* PF_STOP(5); */
    }

}




  /*
   * Seeding the rand()
   */
  void
    srand_core() {
    double timed_ = wtime();
    unsigned int timeprfx_ = (unsigned int) timed_;
    unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
    srand(time_ + (13 * (ID + 1)));
  }


  inline void
    wait_cycles(uint64_t ncycles)
  {
    if (ncycles < 10000)
      {
	uint32_t cy = (((uint32_t) ncycles) / 8);
	while(cy--)
	  {
	    cycle_relax();
	  }
      }
    else
      {
	ticks __end = getticks() + ncycles;
	while (getticks() < __end);
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


#if !defined(NOCM) && !defined(BACKOFF_RETRY)	/* if any other CM (greedy, wholly, faircm) */
static int32_t*
cm_init(nodeid_t node)
{
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

#endif	/* NOCM */
