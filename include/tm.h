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
#define ONCE                            if (RCCE_ue() == 1 || RCCE_num_ues() == 1)


#define BACKOFF
#define BACKOFF_MAX                     3
#define BACKOFF_DELAY                   100

    extern stm_tx_t *stm_tx;
    extern stm_tx_node_t *stm_tx_node;
    extern double duration__;

    extern const char *conflict_reasons[4];
    extern RCCE_COMM RCCE_COMM_APP;

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

/*#define TX_START                                                        \
    { PRINTD("|| Starting new tx");                                     \
    short int reason;                                                   \
    if (reason = sigsetjmp(stm_tx->env, 0)) {                           \
        PRINTD("|| restarting due to %d", reason);                      \
        stm_tx->write_set = write_set_empty(stm_tx->write_set);         \
        stm_tx->read_set = read_set_empty(stm_tx->read_set);            \
        stm_tx->retries++;                                              \
    }*/
#define TX_START                                                        \
    { PRINTD("|| Starting new tx");                                     \
    if (stm_tx == NULL) {                                               \
        stm_tx = tx_metadata_new(RUNNING);                              \
        if (stm_tx == NULL) {                                           \
            PRINTD("Could not alloc tx metadata @ TX_START");           \
            PRINTD("  | FAKE: freeing memory");                         \
            EXIT(-1);                                                   \
        }                                                               \
    }                                                                   \
    short int reason;                                                   \
    if (reason = sigsetjmp(stm_tx->env, 0)) {                           \
        PRINTD("|| restarting due to %d", reason);                      \
        stm_tx->write_set = write_set_empty(stm_tx->write_set);         \
        stm_tx->read_set = read_set_empty(stm_tx->read_set);            \
        if (stm_tx->write_set == NULL || stm_tx->read_set == NULL) {    \
            PRINTD("Could not alloc r/w sets @ TX_START");              \
            PRINTD("  | FAKE: freeing memory");                         \
            EXIT(-1);                                                   \
        }                                                               \
    }                                                                   \
    stm_tx->retries++;

#define TX_ABORT(reason)                                                \
    PRINTD("|| aborting tx");                                           \
    handle_abort(stm_tx, reason);                                       \
    siglongjmp(stm_tx->env, reason);

/*#define TX_COMMIT                                                       \
    PRINTD("|| commiting tx");                                          \
    ps_publish_all();                                                   \
    write_set_persist(stm_tx->write_set);                               \
    ps_finish_all();                                                    \
    mem_info_on_commit(stm_tx->mem_info);                               \
    stm_tx_node->tx_starts += stm_tx->retries;                          \
    stm_tx_node->tx_commited++;                                         \
    stm_tx = tx_metadata_empty(stm_tx); }
*/
    
#define TX_COMMIT                                                       \
    PRINTD("|| commiting tx");                                          \
    ps_publish_all();                                                   \
    write_set_persist(stm_tx->write_set);                               \
    ps_finish_all();                                                    \
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


#define TX_LOAD(addr)                                                   \
    tx_load(stm_tx->write_set, stm_tx->read_set, ((void *) (addr)))

#define TX_STORE(addr, ptr, datatype)                                   \
    write_set_update(stm_tx->write_set, datatype, ((void *) (ptr)), ((void *) (addr)))

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

    inline void * tx_load(write_set_t *ws, read_set_t *rs, void *addr) {
        write_entry_t *we;
        if ((we = write_set_contains(ws, addr)) != NULL) {
            read_set_update(rs, addr);
            return (void *) &we->i;
        }
        else {
            if (!read_set_update(rs, addr)) {
                //the node is NOT already subscribed for the address
                CONFLICT_TYPE conflict;
#ifdef BACKOFF
                unsigned int num_delays = 0;
                unsigned int delay = BACKOFF_DELAY;

retry:
#endif
                if ((conflict = ps_subscribe((void *) addr)) != NO_CONFLICT) {
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
            return addr;
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

