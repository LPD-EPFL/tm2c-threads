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

#ifdef	__cplusplus
extern "C" {
#endif

#define FOR(seconds)					\
  double __ticks_per_sec = 1000000000*REF_SPEED_GHZ;	\
  ticks __duration_ticks = (seconds) * __ticks_per_sec;	\
  ticks __start_ticks = getticks();			\
  ticks __end_ticks = __start_ticks + __duration_ticks;	\
  while ((getticks()) < __end_ticks) {			\
  uint32_t __reps;					\
  for (__reps = 0; __reps < 100; __reps++) {

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
  INLINED long rand_range(long r) {
    int m = RAND_MAX;
    long d, v = 0;

    do {
      d = (m > r ? r : m);
      v += 1 + (long) (d * ((double) rand() / ((double) (m) + 1.0)));
      r -= m;
    } while (r > 0);
    return v;
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

#ifdef PGAS
#  define WSET_EMPTY           write_set_pgas_empty
#  define WSET_PERSIST(stm_tx)
#else
#  define WSET_EMPTY           write_set_empty
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

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
#  define TXRUNNING()     set_tx_running();
#  define TXCOMMITTED()   set_tx_committed();

#  define TXPERSISTING()			\
  if (!set_tx_persisting()) {			\
    TX_ABORT(get_abort_reason());		\
  }

#  define TXCHKABORTED()			\
  if (check_aborted()) {			\
    TX_ABORT(get_abort_reason());		\
  }
#else  /* if no CM */
#  define TXRUNNING()     ;
#  define TXCOMMITTED()   ;
#  define TXPERSISTING()  ;
#  define TXCHKABORTED()	;
#endif	/* NOCM */

#if defined(WHOLLY) || defined(NOCM)
#  define CM_METADATA_INIT_ON_START            ;
#  define CM_METADATA_INIT_ON_FIRST_START      ;
#  define CM_METADATA_UPDATE_ON_COMMIT         ;
#elif defined(FAIRCM)
#  define CM_METADATA_INIT_ON_START            stm_tx->start_ts = getticks();
#  define CM_METADATA_INIT_ON_FIRST_START      ;
#  define CM_METADATA_UPDATE_ON_COMMIT         stm_tx_node->tx_duration += (getticks() - stm_tx->start_ts);
#elif defined(GREEDY)
#  define CM_METADATA_INIT_ON_START            ;
#  define CM_METADATA_INIT_ON_FIRST_START      stm_tx->start_ts = getticks();
#  define CM_METADATA_UPDATE_ON_COMMIT         ;
#else  /* no cm defined */
#  error "One of the contention managers should be selected"
#endif


#define TX_START						\
  { PRINTD("|| Starting new tx");				\
  CM_METADATA_INIT_ON_FIRST_START;				\
  short int reason;						\
  if ((reason = sigsetjmp(stm_tx->env, 0)) != 0) {		\
    PRINTD("|| restarting due to %d", reason);			\
    stm_tx->write_set = WSET_EMPTY(stm_tx->write_set);		\
  }								\
  stm_tx->retries++;						\
  TXRUNNING();							\
  CM_METADATA_INIT_ON_START;

#define TX_ABORT(reason)			\
  PRINTD("|| aborting tx (%d)", reason);	\
  handle_abort(stm_tx, reason);			\
  siglongjmp(stm_tx->env, reason);

#define TX_COMMIT					\
  PRINTD("|| commiting tx");				\
  WLOCKS_ACQUIRE();					\
  TXPERSISTING();					\
  WSET_PERSIST(stm_tx->write_set);			\
  TXCOMMITTED();					\
  ps_finish_all(NO_CONFLICT);				\
  CM_METADATA_UPDATE_ON_COMMIT;				\
  mem_info_on_commit(stm_tx->mem_info);			\
  stm_tx_node->tx_starts += stm_tx->retries;		\
  stm_tx_node->tx_commited++;				\
  stm_tx_node->tx_aborted += stm_tx->aborts;		\
  stm_tx_node->max_retries =				\
    (stm_tx->retries < stm_tx_node->max_retries)	\
    ? stm_tx_node->max_retries				\
    : stm_tx->retries;					\
  stm_tx_node->aborts_war += stm_tx->aborts_war;	\
  stm_tx_node->aborts_raw += stm_tx->aborts_raw;	\
  stm_tx_node->aborts_waw += stm_tx->aborts_waw;	\
  stm_tx = tx_metadata_empty(stm_tx);}

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
  mem_info_on_commit(stm_tx->mem_info);			\
  stm_tx_node->tx_starts += stm_tx->retries;		\
  stm_tx_node->tx_commited++;				\
  stm_tx_node->tx_aborted += stm_tx->aborts;		\
  stm_tx_node->max_retries =				\
    (stm_tx->retries < stm_tx_node->max_retries)	\
    ? stm_tx_node->max_retries				\
    : stm_tx->retries;					\
  stm_tx_node->aborts_war += stm_tx->aborts_war;	\
  stm_tx_node->aborts_raw += stm_tx->aborts_raw;	\
  stm_tx_node->aborts_waw += stm_tx->aborts_waw;	\
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
#define TX_LOAD(addr)					\
  tx_load(stm_tx->write_set, (addr)) /* XXX: if using read_buff -> erro */

#define TX_LOAD_T(addr,type) (type)TX_LOAD(addr)

  /*
   * addr is the local address
   * NONTX acts as a regular load on iRCCE w/o PGAS
   * and fetches data from remote on all other systems
   */
#define NONTX_LOAD(addr)			\
  nontx_load(addr)

#define NONTX_LOAD_T(addr,type) (type)NONTX_LOAD(addr)

#ifdef PGAS
#  ifdef EAGER_WRITE_ACQ
#    define TX_STORE(addr, val, datatype)	\
  do {						\
    tx_wlock(addr, val);			\
  } while (0)
  //not using a write_set in pgas
  //write_set_pgas_update(stm_tx->write_set, val, addr)
#  else /* !EAGER_WRITE_ACQ */
#    define TX_STORE(addr, val, datatype)			\
  do {								\
    tm_intern_addr_t intern_addr = to_intern_addr(addr);	\
    write_set_pgas_update(stm_tx->write_set, val, intern_addr);	\
  } while (0)
#  endif /* EAGER_WRITE_ACQ */

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
#ifdef PGAS

  INLINED int 
  tx_load(write_set_pgas_t *ws, tm_addr_t addr)
#else
    INLINED tm_addr_t 
    tx_load(write_set_t *ws, tm_addr_t addr)
#endif
  {
    tm_intern_addr_t intern_addr = to_intern_addr(addr);

    PREFETCH(intern_addr);

#ifdef PGAS
    //PRINT("(loading: %d)", addr);
    write_entry_pgas_t *we;
    if ((we = write_set_pgas_contains(ws, intern_addr)) != NULL) {
      return we->value;
    }
#else
    write_entry_t *we;
    if ((we = write_set_contains(ws, intern_addr)) != NULL) {
      return (void *) &we->i;
    }
#endif

    else {
	//the node is NOT already subscribed for the address
	CONFLICT_TYPE conflict;
#ifndef BACKOFF_RETRY
	unsigned int num_delays = 0;
	unsigned int delay = BACKOFF_DELAY;

      retry:
#endif
	TXCHKABORTED();
	if ((conflict = ps_subscribe(addr)) != NO_CONFLICT) {
#ifndef BACKOFF_RETRY
	  if (num_delays++ < BACKOFF_MAX) {
	    ndelay(delay);
	    /* ndelay(rand_range(delay)); */
	    delay *= 2;
	    goto retry;
	  }
#endif
	  TX_ABORT(conflict);
	}
#ifdef PGAS
      return read_value;
#else
      return addr;
#endif
    }
  }

  /*  get a tx write lock for address addr
   */
#ifdef PGAS

  INLINED void tx_wlock(tm_addr_t address, int value) {
#else

    INLINED void tx_wlock(tm_addr_t address) {
#endif

      CONFLICT_TYPE conflict;
#ifndef BACKOFF_RETRY
      unsigned int num_delays = 0;
      unsigned int delay = BACKOFF_DELAY;

    retry:
#endif
      TXCHKABORTED();
#ifdef PGAS
      if ((conflict = ps_publish(address, value)) != NO_CONFLICT) {
#else      
	if ((conflict = ps_publish(address)) != NO_CONFLICT) {
#endif
#ifndef BACKOFF_RETRY
	  if (num_delays++ < BACKOFF_MAX) {
	    ndelay(delay);		      
	    /* ndelay(rand_range(delay)); */
	    delay *= 2;
	    goto retry;
	  }
#endif
	  TX_ABORT(conflict);
	}
      }

      /*
       * The non transactional load
       */
#ifdef PGAS

      INLINED uint32_t nontx_load(tm_addr_t addr) {
	return ps_load(addr);
      }
#else /* PGAS */

      INLINED tm_addr_t nontx_load(tm_addr_t addr) {
	// There is no non-PGAS cluster
	return addr;
      }
#endif /* PGAS */

      /*
       * The non transactional store, only in PGAS version
       */
      INLINED void nontx_store(tm_addr_t addr, uint32_t value) {
	return ps_store(addr, value);
      }

#ifdef PGAS

      INLINED void tx_store_inc(tm_addr_t address, int value) {
#else

	INLINED void tx_store_inc(tm_addr_t address) {
#endif

	  CONFLICT_TYPE conflict;
#ifndef BACKOFF_RETRY
	  unsigned int num_delays = 0;
	  unsigned int delay = BACKOFF_DELAY;

	retry:
#endif
	  TXCHKABORTED();
#ifdef PGAS
	  if ((conflict = ps_store_inc(address, value)) != NO_CONFLICT) {
#else      
	    if ((conflict = ps_publish(address)) != NO_CONFLICT) {
#endif
#ifndef BACKOFF_RETRY
	      if (num_delays++ < BACKOFF_MAX) {
		ndelay(delay);
		/* ndelay(rand_range(delay)); */
		delay *= 2;
		goto retry;
	      }
#endif
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

