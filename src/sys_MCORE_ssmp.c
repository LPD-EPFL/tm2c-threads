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
#include <ssmp_send.h>
#ifdef PLATFORM_NUMA
#include <numa.h>
#endif /* PLATFORM_NUMA */
#include "common.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "pgas.h"
#include "mcore_malloc.h"

#include "hash.h"

//#define DEBUG_UTILIZATION
#ifdef  DEBUG_UTILIZATION
unsigned int read_reqs_num = 0, write_reqs_num = 0;
#endif

PS_REPLY* ps_remote_msg; // holds the received msg
static PS_COMMAND *ps_remote;

INLINED nodeid_t min_dsl_id();

INLINED void sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
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
	int place = rank;
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(place, &mask);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
		PRINT("Problem with setting processor affinity: %s\n",
			  strerror(errno));
		EXIT(3);
	}
#ifdef PLATFORM_NUMA
	numa_set_preferred(rank/6);
#endif /* PLATFORM_NUMA */
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
	return MCORE_shmalloc(size);
#endif
}

void
sys_shfree(sys_t_vcharp ptr)
{
	MCORE_shfree(ptr);
}

void
sys_tm_init()
{

}

void
sys_ps_init_(void)
{

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
  cm_abort_flag_mine = cm_init(NODE_ID());
  *cm_abort_flag_mine = NO_CONFLICT;
#endif

  MCORE_shmalloc_init(1024*1024*1024); //1GB

  BARRIERW

  ps_remote_msg = NULL;
  PRINTD("sys_ps_init: done");

  BARRIERW
}

void
sys_dsl_init(void)
{
  BARRIERW

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
  cm_abort_flags = (int32_t **) malloc(TOTAL_NODES() * sizeof(int32_t *));
  assert(cm_abort_flags != NULL);

  uint32_t i;
  for (i = 0; i < TOTAL_NODES(); i++) {
    //TODO: make it open only for app nodes
    cm_abort_flags[i] = cm_init(i);    
  }
#endif

  ps_remote = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
  BARRIERW

}

void
sys_dsl_term(void)
{

}

void
sys_ps_term(void)
{

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

  ssmp_msg_t *msg = (ssmp_msg_t *) &reply;
  ssmp_send(sender, msg, 24);
  //  ssmp_sendm(sender, msg);
  //ssmp_send_inline(sender, msg);
}


#define NB 48
#define BITS 0
unsigned int usages[NB];

void
dsl_communication()
{
  nodeid_t sender, last_recv_from = 0;
  PS_COMMAND_TYPE command;

  ssmp_msg_t *msg;
  msg = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));

  ssmp_color_buf_t *cbuf;
  cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
  if (msg == NULL || cbuf == NULL) {
    PRINT("malloc @ dsl_communication");
    exit(-1);
  }
  ssmp_color_buf_init(cbuf, is_app_core);

  
  int j;
  for (j = 0; j < NB; j++) usages[j] = 0;

  uintptr_t addr_prev = 0; uint32_t req_num = 0;

  while (1) {

    last_recv_from = ssmp_recv_color_start(cbuf, msg, last_recv_from + 1);
    sender = msg->sender;

    ps_remote = (PS_COMMAND *) msg;

    //    usages[hash_tw(ps_remote->address) % NB]++;

    
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
      /*
	PRINT("RL addr: %3d, val: %d", address, PGAS_read(address));
      */
      
      sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
			   ps_remote->address, 
			   PGAS_read(ps_remote->address),
			   try_subscribe(sender, ps_remote->address));
#else
      /* if (NODE_ID() == 0) { */
      /* 	if (sender == 1) { */
      /* 	  printf("[%3d] addr %p \tdiff %d\n", req_num++, ps_remote->address, addr_prev - ps_remote->address); */
      /* 	  addr_prev = ps_remote->address; */
      /* 	} */
      /* } */
      
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
	  write_set_pgas_insert(PGAS_write_sets[sender],
				ps_remote->write_value, 
				ps_remote->address);
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
	  if (NODE_ID() == min_dsl_id()) {
	    print_global_stats();

	    print_hashtable_usage();

	    int j;
	    unsigned long long sumh = 0;
	    for (j = 0; j < NB; j++) sumh+=usages[j];
	    for (j = 0; j < NB; j++) {
	      //	      printf("usages[%02d] = %0.2f [%d]\n", j, 100*(usages[j]/(double)sumh), usages[j]);
	    }
	  }

#ifdef DEBUG_UTILIZATION
	  PRINT("*** Completed requests: %d", read_reqs_num + write_reqs_num);
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

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
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
     if (!ftruncate(abrtfd, cache_line))
       {
	 printf("ftruncate failed\n");
       }
   }

   int32_t *tmp = (int32_t *) mmap(NULL, 64, PROT_READ | PROT_WRITE, MAP_SHARED, abrtfd, 0);
   assert(tmp != NULL);
   
   //   PRINT("-- opened %s @ %p for CM of %d", keyF, tmp, node);

   return tmp;
}
#endif	/* NOCM */
