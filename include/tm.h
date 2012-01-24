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
#include "stm.h"
#include "mem.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define FOR(seconds)                    double starting__ = RCCE_wtime();\
                                            while ((duration__ =\
                                            (RCCE_wtime() - starting__)) < (seconds))

#ifdef DSL
#define ONCE                            if (RCCE_ue() == 1 || RCCE_num_ues() == 1)
#define OTHERS                          if (RCCE_ue() != 1)
#else
#define ONCE                            if (RCCE_ue() == 0 || RCCE_num_ues() == 1)
#endif


#define BACKOFF_
#ifdef BACKOFF
#define BACKOFF_MAX                     3
#define BACKOFF_DELAY                   50
#else
#define BACKOFF_MAX                     6
#define BACKOFF_DELAY                   20
#endif


    extern stm_tx_t *stm_tx;
    extern stm_tx_node_t *stm_tx_node;
    extern double duration__;

    extern const char *conflict_reasons[4];
    extern RCCE_COMM RCCE_COMM_APP;


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
    inline long rand_range(long r) {
        int m = RAND_MAX;
        long d, v = 0;

        do {
            d = (m > r ? r : m);
            v += 1 + (long) (d * ((double) rand() / ((double) (m) + 1.0)));
            r -= m;
        } while (r > 0);
        return v;
    }

    /*
     * Seeding the rand()
     */
    inline void srand_core() {
        double timed_ = RCCE_wtime();
        unsigned int timeprfx_ = (unsigned int) timed_;
        unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
        srand(time_ + (13 * (RCCE_ue() + 1)));
    }


    /*______________________________________________________________________________________________________
     * TM Interface                                                                                         |
     *______________________________________________________________________________________________________|
     */


#define TM_INIT                                                         \
    RCCE_init(&argc, &argv);                                            \
    iRCCE_init();                                                       \
    {                                                                   \
        ID = RCCE_ue();                                                 \
        NUM_UES = RCCE_num_ues();                                       \
        tm_init(ID);

#define TM_INITs                                                        \
    {                                                                   \
        ID = RCCE_ue();                                                 \
        NUM_UES = RCCE_num_ues();                                       \
        tm_init(ID);

#ifdef PGAS
#define WSET_EMPTY           write_set_pgas_empty
#define WSET_PERSIST(stm_tx)
#else
#define WSET_EMPTY           write_set_empty
#define WSET_PERSIST(stm_tx) write_set_persist(stm_tx)
#endif

#define TX_START                                                        \
    { PRINTD("|| Starting new tx");                                     \
    short int reason;                                                   \
    if (reason = sigsetjmp(stm_tx->env, 0)) {                           \
        PRINTD("|| restarting due to %d", reason);                      \
        stm_tx->write_set = WSET_EMPTY(stm_tx->write_set);              \
        stm_tx->read_set = read_set_empty(stm_tx->read_set);            \
    }                                                                   \
    stm_tx->retries++;

#define TX_ABORT(reason)                                                \
    PRINTD("|| aborting tx");                                           \
    handle_abort(stm_tx, reason);                                       \
    siglongjmp(stm_tx->env, reason);

#define TX_COMMIT                                                       \
    PRINTD("|| commiting tx");                                          \
    ps_publish_all();                                                   \
    WSET_PERSIST(stm_tx->write_set);                                    \
    ps_finish_all(NO_CONFLICT);                                         \
    mem_info_on_commit(stm_tx->mem_info);                               \
    stm_tx_node->tx_starts += stm_tx->retries;                          \
    stm_tx_node->tx_commited++;                                         \
    stm_tx_node->tx_aborted += stm_tx->aborts;                          \
    stm_tx_node->max_retries =                                          \
        (stm_tx->retries < stm_tx_node->max_retries)                    \
            ? stm_tx_node->max_retries                                  \
            : stm_tx->retries;                                          \
    stm_tx_node->aborts_war += stm_tx->aborts_war;                      \
    stm_tx_node->aborts_raw += stm_tx->aborts_raw;                      \
    stm_tx_node->aborts_waw += stm_tx->aborts_waw;                      \
    stm_tx = tx_metadata_empty(stm_tx);}

#define TX_COMMIT_NO_STATS                                              \
    PRINTD("|| commiting tx");                                          \
    ps_publish_all();                                                   \
    WSET_PERSIST(stm_tx->write_set);                                    \
    ps_finish_all(NO_CONFLICT);                                         \
    mem_info_on_commit(stm_tx->mem_info);                               \
    stm_tx = tx_metadata_empty(stm_tx);}

#define TX_COMMIT_NO_PUB                                                \
    PRINTD("|| commiting tx");                                          \
    write_set_persist(stm_tx->write_set);                               \
    ps_finish_all(NO_CONFLICT);                                         \
    mem_info_on_commit(stm_tx->mem_info);                               \
    stm_tx_node->tx_starts += stm_tx->retries;                          \
    stm_tx_node->tx_commited++;                                         \
    stm_tx_node->tx_aborted += stm_tx->aborts;                          \
    stm_tx_node->max_retries =                                          \
        (stm_tx->retries < stm_tx_node->max_retries)                    \
            ? stm_tx_node->max_retries                                  \
            : stm_tx->retries;                                          \
    stm_tx_node->aborts_war += stm_tx->aborts_war;                      \
    stm_tx_node->aborts_raw += stm_tx->aborts_raw;                      \
    stm_tx_node->aborts_waw += stm_tx->aborts_waw;                      \
    stm_tx = tx_metadata_empty(stm_tx); }


#define TM_END                                                          \
    PRINTD("|| FAKE: TM ends");                                         \
    ps_send_stats(stm_tx_node, duration__);                             \
    tx_metadata_free(&stm_tx);                                          \
    free(stm_tx_node); }

#define TM_END_STATS                                                    \
    PRINTD("|| FAKE: TM ends");                                         \
    ps_send_stats(stm_tx_node, duration__);                             \
    tx_metadata_free(&stm_tx);                                          \
    tx_metadata_node_print(stm_tx_node);                                \
    free(stm_tx_node); }                                  


#ifdef PGAS
#define TX_LOAD(addr)                                                   \
    tx_load(stm_tx->write_set, stm_tx->read_set, (unsigned int)(addr))
#else
#define TX_LOAD(addr)                                                   \
    tx_load(stm_tx->write_set, stm_tx->read_set, ((void *) (addr)))
#endif

#ifdef PGAS
#define TX_STORE(addr, val)                                             \
    write_set_pgas_update(stm_tx->write_set, val, addr)
#else
#define TX_STORE(addr, ptr, datatype)                                   \
    write_set_update(stm_tx->write_set, datatype, ((void *) (ptr)), ((void *) (addr)))
#endif


#ifdef PGAS
#define TX_LOAD_STORE(addr, op, value)                                  \
        tx_wlock(addr, op(value));
#else
#define TX_LOAD_STORE(addr, op, value, datatype)                        \
    {tx_wlock((void *) (addr));                                         \
    int temp__ = (*(int *) (addr)) op (value);                          \
    write_set_update(stm_tx->write_set, TYPE_INT, &temp__, addr);}
#endif


    /*early release of READ lock -- TODO: the entry remains in read-set, so one
     SHOULD NOT try to re-read the address cause the tx things it keeps the lock*/
#define TX_RRLS(addr)                                                   \
    ps_unsubscribe((void*) (addr));

#define TX_WRLS(addr)                                                   \
    ps_publish_finish((void*) (addr));

    /*__________________________________________________________________________________________
     * TRANSACTIONAL MEMORY ALLOCATION
     * _________________________________________________________________________________________
     */

#define TX_MALLOC(size)                                                   \
    stm_malloc(stm_tx->mem_info, (size_t) size)

#define TX_SHMALLOC(size)                                                 \
    stm_shmalloc(stm_tx->mem_info, (size_t) size)

#define TX_FREE(addr)                                                     \
    stm_free(stm_tx->mem_info, (void *) addr)

#define TX_SHFREE(addr)                                                   \
    stm_shfree(stm_tx->mem_info, (t_vcharp) addr)

    inline void udelay(unsigned int micros) {
        double __ts_end = RCCE_wtime() + ((double) micros / 1000000);
        while (RCCE_wtime() < __ts_end);
    }

    void handle_abort(stm_tx_t *stm_tx, CONFLICT_TYPE reason);

#ifdef PGAS

    inline int tx_load(write_set_pgas_t *ws, read_set_t *rs, unsigned int addr) {
#else

    inline void * tx_load(write_set_t *ws, read_set_t *rs, void *addr) {
#endif
        
#ifdef PGAS
        //PRINT("(loading: %d)", addr);
        write_entry_pgas_t *we;
        if ((we = write_set_pgas_contains(ws, addr)) != NULL) {
            return we->value;
        }
#else
        write_entry_t *we;
        if ((we = write_set_contains(ws, addr)) != NULL) {
            //read_set_update(rs, addr);
            return (void *) &we->i;
        }
#endif

        else {
#ifndef READ_BUF_OFF
            if (!read_set_update(rs, addr)) {
#endif
                //the node is NOT already subscribed for the address
                CONFLICT_TYPE conflict;
#ifdef BACKOFF
                unsigned int num_delays = 1;
                unsigned int delay = BACKOFF_DELAY;

retry:
#endif

#ifdef PGAS
                if ((conflict = ps_subscribe(addr)) != NO_CONFLICT) {
#else
                if ((conflict = ps_subscribe((void *) addr)) != NO_CONFLICT) {
#endif
#ifdef BACKOFF
                    if (num_delays++ < BACKOFF_MAX) {
                        udelay(delay);
                        delay = (2^num_delays) * BACKOFF_DELAY;
                        goto retry;
                    }
#endif
                    TX_ABORT(conflict);
                }
            }
#ifdef PGAS
            return read_value;
#else
            return addr;
#endif
#ifndef READ_BUF_OFF
        }
#endif
    }

    /*  get a tx write lock for address addr
     */
#ifdef PGAS

    inline void tx_wlock(unsigned int address, int value) {
#else

    inline void tx_wlock(void *address) {
#endif

        CONFLICT_TYPE conflict;
#ifdef BACKOFF
        unsigned int num_delays = 0;
        unsigned int delay = BACKOFF_DELAY;

retry:
#endif
#ifdef PGAS
        if ((conflict = ps_store_inc(address, value)) != NO_CONFLICT) {
#else      
        if ((conflict = ps_publish((void *) address)) != NO_CONFLICT) {
#endif
#ifdef BACKOFF
            if (num_delays++ < BACKOFF_MAX) {
                udelay(delay);
                delay *= 2;
                goto retry;
            }
#endif
            TX_ABORT(conflict);
        }
    }

#define taskudelay udelay

    void ps_unsubscribe_all();

    int color(int id, void *aux);

    void tm_init(unsigned int ID);

    void ps_publish_finish_all(unsigned int locked);

    void ps_publish_all();

    void ps_unsubscribe_all();

#ifdef	__cplusplus
}
#endif

#endif	/* TM_H */

