/* 
 * File:   common.h
 * Author: trigonak
 *
 * Created on March 30, 2011, 6:15 PM
 */

#ifndef COMMON_H
#define	COMMON_H

//#define PGAS

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <libconfig.h>

#ifndef INLINED
# if __GNUC__ && !__GNUC_STDC_INLINE__
#  define INLINED static inline __attribute__((always_inline))
# else
#  define INLINED inline
# endif
#endif

#ifndef EXINLINED
#if __GNUC__ && !__GNUC_STDC_INLINE__
#define EXINLINED extern inline
#else
#define EXINLINED inline
#endif
#endif

#include "measurements.h"
#include "tm_types.h"

#ifdef PGAS
#define EAGER_WRITE_ACQ         /*ENABLE eager write lock acquisition*/
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define MAX_NODES 256 /* Maximum expected number of nodes */

#define DSLNDPERNODES   2 /* 1 dedicated DS-Locking core per DSLNDPERNODES cores*/

#define MED printf("[%02d] ", NODE_ID());
#define PRINT(args...) printf("[%02d] ", NODE_ID()); printf(args); printf("\n"); fflush(stdout)
#define PRINTNF(args...) printf("[%02d] ", NODE_ID()); printf(args); printf("\n"); fflush(stdout)
#define PRINTN(args...) printf("[%02d] ", NODE_ID()); printf(args); fflush(stdout)
#define PRINTS(args...)  printf(args);
#define PRINTSF(args...)  printf(args); fflush(stdout)
#define PRINTSME(args...)  printf("[%02d] ", NODE_ID()); printf(args);

#define BMSTART(what) {const char *__bchm_target = what; double __start_time = wtime();
#define BMEND double __end_time = wtime(); ME; printf("[benchmarking] "); printf("%s", __bchm_target);\
        printf(" | %f secs\n", __end_time - __start_time); fflush(stdout);}


#define FLUSH fflush(stdout);
#ifdef DEBUG
#define FLUSHD fflush(stdout);
#define ME printf("%d: ", NODE_ID())
#define PRINTF(args...) printf(args)
#define PRINTD(args...) ME; printf(args); printf("\n"); fflush(stdout)
#define PRINTDNN(args...) ME; printf(args); fflush(stdout)
#define PRINTD1(UE, args...) if(NODE_ID() == (UE)) { ME; printf(args); printf("\n"); fflush(stdout); }
#define TS printf("[%f] ", wtime())
#else
#define FLUSHD
#define ME
#define PRINTF(args...)
#define PRINTD(args...)
#define PRINTDNN(args...)
#define PRINTD1(UE, args...)
#define TS
#endif

    typedef enum {
        NO_CONFLICT,
        READ_AFTER_WRITE,
        WRITE_AFTER_READ,
        WRITE_AFTER_WRITE
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
    
/*  ------- Plug platform related things here BEGIN ------- */
#ifdef PLATFORM_iRCCE
#include "iRCCE.h"
#include "sys_iRCCE.h"
extern RCCE_COMM RCCE_COMM_APP;
#define BARRIER RCCE_barrier(&RCCE_COMM_APP);
#define BARRIERW RCCE_barrier(&RCCE_COMM_WORLD);

#endif 

#ifdef PLATFORM_CLUSTER
extern void*  zmq_context;             // here, we keep the context (seems necessary)
extern void*  the_responder;           // for dsl nodes, it will be the responder socket; for app node, it will be NULL

extern void*  zmq_barrier_subscriber; // socket for the barrier subscriber
extern void*  zmq_barrier_client;     // socket for applying for the barrier

EXINLINED char *zmq_s_recv(void *socket);
EXINLINED int zmq_s_send(void *socket, char *string);
#define BARRIER_(type)    do {                                               \
    PRINT("BARRIER %s\n", type);                                             \
    if ((ID % DSLNDPERNODES == 0) && !strcmp(type,"app")) break;             \
    PRINT("Waiting at the barrier\n");                                       \
    zmq_s_send(zmq_barrier_client, type);                                    \
    char *received = zmq_s_recv(zmq_barrier_client);                         \
    free(received);                                                          \
    received = zmq_s_recv(zmq_barrier_subscriber);                           \
    free(received);                                                          \
    PRINT("Passed through the barrier\n");                                   \
} while (0);

#define BARRIER    BARRIER_("app")
#define BARRIERW   BARRIER_("all")
#endif

#ifdef PLATFORM_MCORE

#include "sys_MCORE.h"
#endif

#ifdef PLATFORM_TILERA
	/*
	 * TILERA related includes
	 */
#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/task.h>
#include <tmc/udn.h>
#include <tmc/spin.h>
#include <tmc/sync.h>
#include <tmc/cmem.h>

extern DynamicHeader *udn_header; //headers for messaging
extern tmc_sync_barrier_t *barrier_apps, *barrier_all; //BARRIERS

#define BARRIER tmc_sync_barrier_wait(barrier_apps); //app cores only
#define BARRIERW tmc_sync_barrier_wait(barrier_all); //all cores

INLINED double RCCE_wtime() {
	struct timeval t;
	gettimeofday(&t, NULL);
	return (double) t.tv_sec + ((double) t.tv_usec) / 1000000.0;
}

#define RCCE_num_ues TOTAL_NODES
#define RCCE_ue NODE_ID
#define RCCE_acquire_lock(a) 
#define RCCE_release_lock(a)
#define RCCE_init(a,b)
#define iRCCE_init(a)

#define t_vcharp sys_t_vcharp

#endif

/*  ------- Plug platform related things here END   ------- */

#define TASKMAIN MAIN
#ifdef LIBTASK
#define MAIN void taskmain
#else
#define MAIN int main
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

