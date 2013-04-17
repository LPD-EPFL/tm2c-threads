/*
 * File:   tm.h
 * Author: trigonak
 *
 * Created on April 13, 2011, 9:58 AM
 * 
 * The TM interface to the user
 */

#ifndef TM_H
#define	TM_H

#include <setjmp.h>
#include "common.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "stm.h"
#include "mem.h"
#ifdef PGAS
#  include "pgas_app.h"
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define FOR_ITERS(iters)					\
  double __start = wtime();					\
    uint32_t __iterations;					\
    for (__iterations; __iterations < (iters); __iterations++)	\

#define END_FOR_ITERS				\
  duration__ = wtime() - __start;

#define FOR(seconds)						\
  double __ticks_per_sec = 1000000000*REF_SPEED_GHZ;		\
    ticks __duration_ticks = (seconds) * __ticks_per_sec;	\
    ticks __start_ticks = getticks();				\
    ticks __end_ticks = __start_ticks + __duration_ticks;	\
    while ((getticks()) < __end_ticks) {			\
      uint32_t __reps;						\
      for (__reps = 0; __reps < 1000; __reps++) {

#define END_FOR						\
  }}							\
    __end_ticks = getticks();				\
    __duration_ticks = __end_ticks - __start_ticks;	\
    duration__ = __duration_ticks / __ticks_per_sec;


#define FOR_SEC(seconds)			\
  double __start = wtime();			\
  double __end = __start + (seconds);		\
  while (wtime() < __end) {			\
  uint32_t __reps;				\
  for (__reps = 0; __reps < 100; __reps++) {

#define END_FOR_SEC				\
  }}						\
    __end = wtime();				\
    duration__ = __end - __start;


#define APP_EXEC_ORDER				\
  {						\
  int i;					\
  for (i = 0; i < TOTAL_NODES(); i++)		\
    {						\
  if (i == NODE_ID())				\
    {						
  
#define APP_EXEC_ORDER_END			\
  }						\
    BARRIER;}}

#define DSL_EXEC_ORDER				\
  {						\
  int i;					\
  for (i = 0; i < TOTAL_NODES(); i++)		\
    {						\
  if (i == NODE_ID())				\
    {						
  
#define DSL_EXEC_ORDER_END			\
  }						\
    BARRIER_DSL;}}


  extern stm_tx_t *stm_tx;
  extern stm_tx_node_t *stm_tx_node;
  extern double duration__;

  extern const char *conflict_reasons[4];

  /*______________________________________________________________________________
   * Help functions
   *______________________________________________________________________________
   */

  /* 
   * Returns a pseudo-random value in [1;range).
   * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
   * the granularity of rand() could be lower-bounded by the 32767^th which might 
   * be too high for given values of range and initial.
   */
  INLINED long 
  rand_range(long int r) 
  {
    int m = RAND_MAX;
    long d, v = 0;
    do 
      {
	d = (m > r ? r : m);
	v += 1 + (long) (d * ((double) rand() / ((double) (m) + 1.0)));
	r -= m;
      } while (r > 0);
    
    return v;
  }



  //Marsaglia's xorshf generator
  //period 2^96-1
  INLINED unsigned long
  xorshf96(unsigned long* x, unsigned long* y, unsigned long* z) 
  {
    unsigned long t;
    (*x) ^= (*x) << 16;
    (*x) ^= (*x) >> 5;
    (*x) ^= (*x) << 1;

    t = *x;
    (*x) = *y;
    (*y) = *z;
    (*z) = t ^ (*x) ^ (*y);
    return *z;
  }

  INLINED unsigned long
  tm2c_rand()
  {
    return xorshf96(tm2c_rand_seeds, tm2c_rand_seeds + 1, tm2c_rand_seeds + 2);
  }

  INLINED uint32_t
  pow2roundup (uint32_t x)
  {
    if (x==0) 
      {
	return 1;
      }
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
  }


  /*______________________________________________________________________________________________________
   * TM Interface                                                                                         |
   *______________________________________________________________________________________________________|
   */

  //  init_configuration(&argc, &argv);		\

#define TM_INIT					\
  init_system(&argc, &argv);			\
  {						\
  tm_init();

#define TM_INITs				\
  {						\
  tm_init();

#define SEQ_INIT				\
  init_system(&argc, &argv);			\
  sys_ps_init_();				\
  tm2c_rand_seeds = seed_rand();

#ifdef PGAS
#  define WSET                  NULL
#  define WSET_EMPTY
#  define WSET_PERSIST(stm_tx)
#else
#  define WSET                  stm_tx->write_set
#  define WSET_EMPTY           WSET = write_set_empty(WSET)
#  define WSET_PERSIST(stm_tx) write_set_persist(stm_tx)
#endif

#ifdef EAGER_WRITE_ACQ
#  define WLOCKS_ACQUIRE()
#else
#  define WLOCKS_ACQUIRE()        ps_publish_all()
#endif

  /* -------------------------------------------------------------------------------- */
  /* Contention management related macros */
  /* -------------------------------------------------------------------------------- */

#if !defined(NOCM) && !defined(BACKOFF_RETRY) /* if any other CM (greedy, wholly, faircm) */
#  define TXRUNNING()     set_tx_running();
#  define TXCOMMITTED()   set_tx_committed();

#  define TXPERSISTING()			\
  if (!set_tx_persisting())			\
    {						\
      TX_ABORT(get_abort_reason());		\
    }

#  define TXCHKABORTED()			\
  if (check_aborted())				\
    {						\
      TX_ABORT(get_abort_reason());		\
    }
#else  /* if no CM or BACKOFF_RETRY */
#  define TXRUNNING()     ;
#  define TXCOMMITTED()   ;
#  define TXPERSISTING()  ;
#  define TXCHKABORTED()  ;
#endif	/* NOCM */

#if defined(WHOLLY) || defined(NOCM) || defined(BACKOFF_RETRY)
#  define CM_METADATA_INIT_ON_START            ;
#  define CM_METADATA_INIT_ON_FIRST_START      ;
#  define CM_METADATA_UPDATE_ON_COMMIT         ;
#elif defined(FAIRCM)
#  define CM_METADATA_INIT_ON_START            stm_tx->start_ts = getticks();
#  define CM_METADATA_INIT_ON_FIRST_START      ;
#  define CM_METADATA_UPDATE_ON_COMMIT         stm_tx_node->tx_duration += (getticks() - stm_tx->start_ts);
#elif defined(GREEDY)
#  define CM_METADATA_INIT_ON_START            ;
#  ifdef GREEDY_GLOBAL_TS	/* use fetch and increment */
#    define CM_METADATA_INIT_ON_FIRST_START      stm_tx->start_ts = greedy_get_global_ts();
#  else
#    define CM_METADATA_INIT_ON_FIRST_START      stm_tx->start_ts = getticks();
#  endif
#  define CM_METADATA_UPDATE_ON_COMMIT         ;
#else  /* no cm defined */
#  error "One of the contention managers should be selected"
#endif


#define TX_START					\
  { PRINTD("|| Starting new tx");			\
  CM_METADATA_INIT_ON_FIRST_START;			\
  short int reason;					\
  if ((reason = sigsetjmp(stm_tx->env, 0)) != 0) {	\
    PRINTD("|| restarting due to %d", reason);		\
    WSET_EMPTY;						\
  }							\
  stm_tx->retries++;					\
  TXRUNNING();						\
  CM_METADATA_INIT_ON_START;

#define TX_ABORT(reason)			\
  PRINTD("|| aborting tx (%d)", reason);	\
  handle_abort(stm_tx, reason);			\
  siglongjmp(stm_tx->env, reason);

#define TX_COMMIT				\
  WLOCKS_ACQUIRE();				\
  TXPERSISTING();				\
  WSET_PERSIST(stm_tx->write_set);		\
  TXCOMMITTED();				\
  ps_finish_all(NO_CONFLICT);			\
  CM_METADATA_UPDATE_ON_COMMIT;			\
  stm_tx_node->tx_starts++;			\
  stm_tx_node->tx_commited++;			\
  stm_tx_node->tx_aborted += stm_tx->aborts;	\
  stm_tx = tx_metadata_empty(stm_tx);}


#define TX_COMMIT_MEM				\
  WLOCKS_ACQUIRE();				\
  TXPERSISTING();				\
  WSET_PERSIST(stm_tx->write_set);		\
  TXCOMMITTED();				\
  ps_finish_all(NO_CONFLICT);			\
  CM_METADATA_UPDATE_ON_COMMIT;			\
  mem_info_on_commit(stm_tx->mem_info);		\
  stm_tx_node->tx_starts++;			\
  stm_tx_node->tx_commited++;			\
  stm_tx_node->tx_aborted += stm_tx->aborts;	\
  stm_tx = tx_metadata_empty(stm_tx);}


/* #define TX_COMMIT					\ */
/*   stm_tx_node->max_retries =				\ */
/*     (stm_tx->retries < stm_tx_node->max_retries)	\ */
/*     ? stm_tx_node->max_retries				\ */
/*     : stm_tx->retries;					\ */
/*   stm_tx_node->aborts_war += stm_tx->aborts_war;	\ */
/*   stm_tx_node->aborts_raw += stm_tx->aborts_raw;	\ */
/*   stm_tx_node->aborts_waw += stm_tx->aborts_waw;	\ */


#define TX_COMMIT_NO_STATS			\
  PRINTD("|| commiting tx");			\
  WLOCKS_ACQUIRE();				\
  TXPERSISTING();				\
  WSET_PERSIST(stm_tx->write_set);		\
  TXCOMMITTED();				\
  ps_finish_all(NO_CONFLICT);			\
  CM_METADATA_UPDATE_ON_COMMIT;			\
  mem_info_on_commit(stm_tx->mem_info);		\
  stm_tx_node->tx_commited++;			\
  stm_tx = tx_metadata_empty(stm_tx);}

#define TX_COMMIT_NO_PUB_NO_STATS		\
  TXPERSISTING();				\
  WSET_PERSIST(stm_tx->write_set);		\
  TXCOMMITTED();				\
  ps_finish_all(NO_CONFLICT);			\
  CM_METADATA_UPDATE_ON_COMMIT;			\
  mem_info_on_commit(stm_tx->mem_info);		\
  stm_tx = tx_metadata_empty(stm_tx);}

#define TX_COMMIT_NO_PUB				\
  PRINTD("|| commiting tx");				\
  TXPERSISTING();					\
  WSET_PERSIST(stm_tx->write_set);			\
  TXCOMMITTED();					\
  ps_finish_all(NO_CONFLICT);				\
  CM_METADATA_UPDATE_ON_COMMIT;				\
  stm_tx_node->tx_starts += stm_tx->retries;		\
  stm_tx_node->tx_commited++;				\
  stm_tx_node->tx_aborted += stm_tx->aborts;		\
  stm_tx = tx_metadata_empty(stm_tx); }


#define TM_END					\
  PRINTD("|| FAKE: TM ends");			\
  ps_send_stats(stm_tx_node, duration__);	\
  TM_TERM;}

  /*
    tx_metadata_free(&stm_tx);                                          \
    free(stm_tx_node); }
  */

#define TM_END_STATS				\
  PRINTD("|| FAKE: TM ends");			\
  ps_send_stats(stm_tx_node, duration__);	\
  tx_metadata_free(&stm_tx);			\
  tx_metadata_node_print(stm_tx_node);		\
  free(stm_tx_node); }                                  

#define TM_TERM					\
  PRINTD("|| FAKE: TM terminates");		\
  tm_term();					\
  term_system();

  /*
   * addr is the local address
   */
#if defined(PGAS)
#  define TX_LOAD(addr, words)			\
  tx_load(WSET, (addr), words) 
#else
#  define TX_LOAD(addr)				\
  tx_load(WSET, (addr)) 
#endif	/* PGAS */

#define TX_LOAD_T(addr,type) (type)TX_LOAD(addr)

  /*
   * addr is the local address
   * NONTX acts as a regular load on iRCCE w/o PGAS
   * and fetches data from remote on all other systems
   */
#if defined(PGAS)
#  define NONTX_LOAD(addr, words)		\
  nontx_load(addr, words)
#else  /* !PGAS */
#  define NONTX_LOAD(addr)			\
  nontx_load(addr)
#endif	/* PGAS */


#define NONTX_LOAD_T(addr,type) (type)NONTX_LOAD(addr)

#if defined(PGAS)
#  define TX_STORE(addr, val, datatype)		\
  do {						\
    tx_wlock(addr, val);			\
  } while (0)
  //not using a write_set in pgas
  //write_set_pgas_update(stm_tx->write_set, val, addr)
#else /* !PGAS */
#  define TX_STORE(addr, val, datatype)				\
  do {								\
    TXCHKABORTED();						\
    tm_intern_addr_t intern_addr = to_intern_addr(addr);	\
    write_set_insert(stm_tx->write_set,				\
		     datatype,					\
		     val, intern_addr);				\
  } while (0)
#endif

#define TX_CAS(addr, oldval, newval)		\
  tx_cas(addr, oldval, newval);

#define TX_CASI(addr, oldval, newval)		\
  tx_casi(addr, oldval, newval);


#if defined(PLATFORM_CLUSTER) || defined(PGAS)
#  define NONTX_STORE(addr, val, datatype)	\
  nontx_store(addr, val)
#else 
#  define NONTX_STORE(addr, val, datatype)	\
  *((__typeof__ (val)*)(addr)) = (val)
#endif

#ifdef PGAS
#  define TX_LOAD_STORE(addr, op, value, datatype)	\
  do { tx_store_inc(addr, op(value)); } while (0)
#else
#  define TX_LOAD_STORE(addr, op, value, datatype)			\
  do {									\
    tx_wlock(addr);							\
    int temp__ = (*(int *) (addr)) op (value);				\
    tm_intern_addr_t intern_addr = to_intern_addr((tm_addr_t)addr);	\
    write_set_insert(stm_tx->write_set, TYPE_INT, temp__, intern_addr); \
  } while (0)
#endif

#define DUMMY_MSG(to)				\
  ps_dummy_msg(to)
  


  /*early release of READ lock -- TODO: the entry remains in read-set, so one
    SHOULD NOT try to re-read the address cause the tx things it keeps the lock*/
#define TX_RRLS(addr)				\
  ps_unsubscribe((void*) (addr));

#define TX_WRLS(addr)				\
  ps_publish_finish((void*) (addr));

  /*__________________________________________________________________________________________
   * TRANSACTIONAL MEMORY ALLOCATION
   * _________________________________________________________________________________________
   */

#define TX_MALLOC(size)				\
  stm_malloc(stm_tx->mem_info, (size_t) size)

#define TX_SHMALLOC(size)			\
  stm_shmalloc(stm_tx->mem_info, (size_t) size)

#define TX_FREE(addr)				\
  stm_free(stm_tx->mem_info, (void *) addr)

#define TX_SHFREE(addr)				\
  stm_shfree(stm_tx->mem_info, (t_vcharp) addr)

  void handle_abort(stm_tx_t *stm_tx, CONFLICT_TYPE reason);

  /*
   * API function
   * accepts the machine-local address, which must be translated to the appropriate
   * internal address
   */
#if defined(PGAS)
  INLINED int64_t
  tx_load(write_set_pgas_t *ws, tm_addr_t addr, int words)
  {
    tm_intern_addr_t intern_addr = to_intern_addr(addr);
    CONFLICT_TYPE conflict;
#  ifndef BACKOFF_RETRY
    unsigned int num_delays = 0;
    unsigned int delay = BACKOFF_DELAY;

  retry:
#  endif
    TXCHKABORTED();
    if ((conflict = ps_subscribe(addr, words)) != NO_CONFLICT) 
      {
#  ifndef BACKOFF_RETRY
	if (num_delays++ < BACKOFF_MAX) 
	  {
	    ndelay(delay);
	    /* ndelay(rand_range(delay)); */
	    delay *= 2;
	    goto retry;
	  }
#  endif
	TX_ABORT(conflict);
      }

    return read_value;
  }

#else  /* !PGAS */
  INLINED tm_addr_t 
  tx_load(write_set_t *ws, tm_addr_t addr)
  {
    tm_intern_addr_t intern_addr = to_intern_addr(addr);
#  if defined(PLATFORM_MCORE) || defined(PLATFORM_MCORE_SSMP)
    PREFETCH(intern_addr);
#  endif
    write_entry_t *we;
    if ((we = write_set_contains(ws, intern_addr)) != NULL) 
      {
	return (void *) &we->i;
      }
    else 
      {
	//the node is NOT already subscribed for the address
	CONFLICT_TYPE conflict;
/* #  ifndef BACKOFF_RETRY */
/* 	unsigned int num_delays = 0; */
/* 	unsigned int delay = BACKOFF_DELAY; */

/*       retry: */
/* #  endif */
	TXCHKABORTED();
	if ((conflict = ps_subscribe(addr, 0)) != NO_CONFLICT) /* 0 for the #words used in PGAS */
	  {
/* #  ifndef BACKOFF_RETRY */
/* 	    if (num_delays++ < BACKOFF_MAX)  */
/* 	      { */
/* 		ndelay(delay); */
/* 		/\* ndelay(rand_range(delay)); *\/ */
/* 		delay *= 2; */
/* 		goto retry; */
/* 	      } */
/* #  endif */
	    TX_ABORT(conflict);
	  }
	/* TXCHKABORTED(); */
	return addr;
      }
  }
#endif	/* PGAS */
  /*  get a tx write lock for address addr
   */
#ifdef PGAS

  INLINED void tx_wlock(tm_addr_t address, int64_t value) {
#else

    INLINED void tx_wlock(tm_addr_t address) {
#endif

      CONFLICT_TYPE conflict;
/* #ifndef BACKOFF_RETRY */
/*       unsigned int num_delays = 0; */
/*       unsigned int delay = BACKOFF_DELAY; */

/*     retry: */
/* #endif */
      TXCHKABORTED();
#ifdef PGAS
      if ((conflict = ps_publish(address, value)) != NO_CONFLICT) {
#else      
	if ((conflict = ps_publish(address)) != NO_CONFLICT) {
#endif
/* #ifndef BACKOFF_RETRY */
/* 	  if (num_delays++ < BACKOFF_MAX) { */
/* 	    ndelay(delay);		       */
/* 	    /\* ndelay(rand_range(delay)); *\/ */
/* 	    delay *= 2; */
/* 	    goto retry; */
/* 	  } */
/* #endif */
	  TX_ABORT(conflict);
	}
      }

      /*
       * The non transactional load
       */
#ifdef PGAS

      INLINED int64_t nontx_load(tm_addr_t addr, unsigned int words) 
      {
	tm_addr_t ab = addr;
	int64_t v = ps_load(addr, words);
	return v;
      }
#else /* !PGAS */

      INLINED tm_addr_t nontx_load(tm_addr_t addr) {
	// There is no non-PGAS cluster
	return addr;
      }
#endif /* PGAS */

      /*
       * The non transactional store, only in PGAS version
       */
      INLINED void 
	nontx_store(tm_addr_t addr, int64_t value) 
      {
	return ps_store(addr, value);
      }

#ifdef PGAS
      INLINED void tx_store_inc(tm_addr_t address, int64_t value) {
#else
	INLINED void tx_store_inc(tm_addr_t address) {
#endif

	  CONFLICT_TYPE conflict;
/* #ifndef BACKOFF_RETRY */
/* 	  unsigned int num_delays = 0; */
/* 	  unsigned int delay = BACKOFF_DELAY; */

/* 	retry: */
/* #endif */
	  TXCHKABORTED();
#ifdef PGAS
	  if ((conflict = ps_store_inc(address, value)) != NO_CONFLICT) {
#else      
	    if ((conflict = ps_publish(address)) != NO_CONFLICT) {
#endif
/* #ifndef BACKOFF_RETRY */
/* 	      if (num_delays++ < BACKOFF_MAX) { */
/* 		ndelay(delay); */
/* 		/\* ndelay(rand_range(delay)); *\/ */
/* 		delay *= 2; */
/* 		goto retry; */
/* 	      } */
/* #endif */
	      TX_ABORT(conflict);
	    }
	  }

	  uint32_t tx_cas(tm_addr_t addr, uint32_t oldval, uint32_t newval);
	  extern inline uint32_t tx_casi(tm_addr_t addr, uint32_t oldval, uint32_t newval);


#define taskudelay udelay

	  void ps_unsubscribe_all();

	  int is_app_core(int id);

	  void init_system(int* argc, char** argv[]);

	  void tm_init();
	  void tm_term();

	  void ps_publish_finish_all(unsigned int locked);

	  void ps_publish_all();

	  void ps_unsubscribe_all();

#ifdef	__cplusplus
	}
#endif

#endif	/* TM_H */

