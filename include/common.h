/* 
 * File:   common.h
 * Author: trigonak
 *
 * Created on March 30, 2011, 6:15 PM
 */

#ifndef COMMON_H
#define	COMMON_H

#ifdef	__cplusplus
extern "C" {
#endif

  //#define PGAS

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <inttypes.h>
#include <libconfig.h>

#ifndef INLINED
#  if __GNUC__ && !__GNUC_STDC_INLINE__ && !SCC
#    define INLINED static inline __attribute__((always_inline))
#  else
#    define INLINED inline
#  endif
#endif

#ifndef EXINLINED
#  if __GNUC__ && !__GNUC_STDC_INLINE__ && !SCC
#    define EXINLINED extern inline
#  else
#    define EXINLINED inline
#  endif
#endif

#ifndef ALIGNED
#  if __GNUC__ && !SCC
#    define ALIGNED(N) __attribute__ ((aligned (N)))
#  else
  //#    warning "The alignement of elemenets should be explicitly handled"
#    define ALIGNED(N)
#  endif
#endif

#include "tm_types.h"

#ifdef PGAS
#  define EAGER_WRITE_ACQ         /*ENABLE eager write lock acquisition*/
#endif

#define MAX_NODES 256 /* Maximum expected number of nodes */

#define DSLNDPERNODES   2 /* 1 dedicated DS-Locking core per DSLNDPERNODES cores*/


#  define CACHE_LINE_SIZE 64

#if defined(PLATFORM_MCORE)
#  define REF_SPEED_GHZ           2.1
#elif defined(SCC)
#  define REF_SPEED_GHZ           0.533
#elif defined(PLATFORM_TILERA)
#  define REF_SPEED_GHZ           0.7
#else
#  error "Need to set REF_SPEED_GHZ for the platform"
#endif

#define MED printf("[%02d] ", NODE_ID());
#define PRINT(args...) printf("[%02d] ", NODE_ID()); printf(args); printf("\n"); fflush(stdout)
#define PRINTNF(args...) printf("[%02d] ", NODE_ID()); printf(args); printf("\n"); fflush(stdout)
#define PRINTN(args...) printf("[%02d] ", NODE_ID()); printf(args); fflush(stdout)
#define PRINTS(args...)  printf(args);
#define PRINTSF(args...)  printf(args); fflush(stdout)
#define PRINTSME(args...)  printf("[%02d] ", NODE_ID()); printf(args);

#define BMSTART(what) {const char *__bchm_target = what; double __start_time = wtime();
#define BMEND double __end_time = wtime(); ME; printf("[benchmarking] "); printf("%s", __bchm_target); \
  printf(" | %f secs\n", __end_time - __start_time); fflush(stdout);}


#define FLUSH fflush(stdout);
#ifdef DEBUG
#  define FLUSHD fflush(stdout);
#  define ME printf("%d: ", NODE_ID())
#  define PRINTF(args...) printf(args)
#  define PRINTD(args...) ME; printf(args); printf("\n"); fflush(stdout)
#  define PRINTDNN(args...) ME; printf(args); fflush(stdout)
#  define PRINTD1(UE, args...) if(NODE_ID() == (UE)) { ME; printf(args); printf("\n"); fflush(stdout); }
#  define TS printf("[%f] ", wtime())
#else
#  define FLUSHD
#  define ME
#  define PRINTF(args...)
#  define PRINTD(args...)
#  define PRINTDNN(args...)
#  define PRINTD1(UE, args...)
#  define TS
#endif

  typedef enum {
    NO_CONFLICT,
    READ_AFTER_WRITE,
    WRITE_AFTER_READ,
    WRITE_AFTER_WRITE,
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
    PERSISTING_WRITES, 	/* used for contention management */
    TX_COMMITED,		/* used for contention management */
#endif 				/* NOCM */
    CAS_SUCCESS
  } CONFLICT_TYPE;

  /* read or write request
   */
  typedef enum {
    READ,
    WRITE
  } RW;

  extern nodeid_t ID;
  extern nodeid_t NUM_UES;
  extern nodeid_t NUM_APP_NODES;
  extern nodeid_t NUM_DSL_NODES;
    

  extern int is_app_core(int);
  extern int is_dsl_core(int);

  INLINED nodeid_t
  min_dsl_id() 
  {
    uint32_t i;
    for (i = 0; i < NUM_UES; i++)
      {
	if (!is_app_core(i))
	  {
	    return i;
	  }
      }
    return i;
  }

  /*
    returns the posisition of the core id in the seq of dsl cores
    e.g., having 6 cores total and core 2 and 4 are dsl, then
    the call to this function with node=2=>0, with node=4=>1
  */
  INLINED nodeid_t
  dsl_id_seq(nodeid_t node) 
  {
    uint32_t i, seq = 0;
    for (i = 0; i < node; i++)
      {
	if (!is_app_core(i))
	  {
	    seq++;
	  }
      }
    return seq;
  }

  /*
    returns the posisition of the core id in the seq of dsl cores
    e.g., having 6 cores total and core 2 and 4 are dsl, then
    the call to this function with node=2=>0, with node=4=>1
  */
  INLINED nodeid_t
  app_id_seq(nodeid_t node) 
  {
    uint32_t i, seq = 0;
    for (i = 0; i < node; i++)
      {
	if (is_app_core(i))
	  {
	    seq++;
	  }
      }
    return seq;
  }



  INLINED nodeid_t
  min_app_id() 
  {
    uint32_t i;
    for (i = 0; i < NUM_UES; i++)
      {
	if (is_app_core(i))
	  {
	    return i;
	  }
      }
    return i;
  }

#define ONCE						\
  if (is_app_core(ID) && NODE_ID() == min_app_id() ||	\
      ID == min_dsl_id() ||				\
      NUM_UES == 1)


#if defined(PLATFORM_TILERA)	/* need it before measurements.h */
  typedef uint64_t ticks;
#endif	/* PLATFORM_TILERA */

#include "measurements.h"

  /*  ------- Plug platform related things here BEGIN ------- */
#if defined(PLATFORM_iRCCE) || defined(PLATFORM_SCC_SSMP)

#  ifdef SSMP
#    include "RCCE.h"
#    include "sys_SCC_ssmp.h"
#  else
#    include "iRCCE.h"
#    include "sys_iRCCE.h"
  extern RCCE_COMM RCCE_COMM_APP;
#    define BARRIER RCCE_barrier(&RCCE_COMM_APP);
#    define BARRIERW RCCE_barrier(&RCCE_COMM_WORLD);
#    define BARRIER_DSL ; 	/* XXX: to implement */
#  endif

#endif 

#ifdef PLATFORM_CLUSTER
  extern void*  zmq_context;             // here, we keep the context (seems necessary)
  extern void*  the_responder;           // for dsl nodes, it will be the responder socket; for app node, it will be NULL

  extern void*  zmq_barrier_subscriber; // socket for the barrier subscriber
  extern void*  zmq_barrier_client;     // socket for applying for the barrier

  EXINLINED char *zmq_s_recv(void *socket);
  EXINLINED int zmq_s_send(void *socket, char *string);
#  define BARRIER_(type)    do {					\
    PRINT("BARRIER %s\n", type);					\
    if ((ID % DSLNDPERNODES == 0) && !strcmp(type,"app")) break;	\
    PRINT("Waiting at the barrier\n");					\
    zmq_s_send(zmq_barrier_client, type);				\
    char *received = zmq_s_recv(zmq_barrier_client);			\
    free(received);							\
    received = zmq_s_recv(zmq_barrier_subscriber);			\
    free(received);							\
    PRINT("Passed through the barrier\n");				\
  } while (0);

#  define BARRIER    BARRIER_("app")
#  define BARRIERW   BARRIER_("all")
#endif

#ifdef PLATFORM_MCORE

#  ifndef SSMP
#    include "sys_MCORE.h"
#  else
#    include "sys_MCORE_ssmp.h"
#  endif
#endif

#ifdef PLATFORM_TILERA
  /*
   * TILERA related includes
   */
#  include <tmc/alloc.h>
#  include <tmc/cpus.h>
#  include <tmc/task.h>
#  include <tmc/udn.h>
#  include <tmc/spin.h>
#  include <tmc/sync.h>
#  include <tmc/cmem.h>
#  include <arch/cycle.h> 

#  include "sys_TILERA.h"


  extern DynamicHeader *udn_header; //headers for messaging
  extern tmc_sync_barrier_t *barrier_apps, *barrier_all; //BARRIERS

#  define BARRIER tmc_sync_barrier_wait(barrier_apps); //app cores only
#  define BARRIERW tmc_sync_barrier_wait(barrier_all); //all cores
#  define BARRIER_DSL tmc_sync_barrier_wait(barrier_dsl); //all cores

#  define getticks get_cycle_count

#  define RCCE_num_ues TOTAL_NODES
#  define RCCE_ue NODE_ID
#  define RCCE_acquire_lock(a) 
#  define RCCE_release_lock(a)
#  define RCCE_init(a,b)
#  define iRCCE_init(a)

#  define t_vcharp sys_t_vcharp

#endif

  /*  ------- Plug platform related things here END   ------- */

#define TASKMAIN MAIN
#ifdef LIBTASK
#  define MAIN void taskmain
#else
#  define MAIN int main
#endif
#define EXIT(reason) exit(reason);
#define EXITALL(reason) exit((reason))

  // configuration...
  extern config_t *the_config;
  void init_configuration(int*argc, char**argv[]);

#include "tm_sys.h"

#ifdef	__cplusplus
}
#endif

#endif	/* COMMON_H */

