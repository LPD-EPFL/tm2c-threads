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

#ifdef	__cplusplus
extern "C" {
#endif

#define BACKOFF
#define BACKOFF_MAX 3
#define BACKOFF_DELAY 400

    const char *conflict_reasons[4] = {
        "NO_CONFLICT",
        "READ_AFTER_WRITE",
        "WRITE_AFTER_READ",
        "WRITE_AFTER_WRITE"
    };

    /*______________________________________________________________________________________________________
     * TM Interface                                                                                         |
     *______________________________________________________________________________________________________|
     */


#define TM_INIT                                                         \
    RCCE_init(&argc, &argv);                                            \
    iRCCE_init();                                                       \
    RCCE_COMM RCCE_COMM_APP;                                            \
    RCCE_comm_split(color, NULL, &RCCE_COMM_APP);                       \
int comsz; RCCE_comm_size(RCCE_COMM_APP, &comsz);PRINTD("size of comm_app is %d", comsz);\
    {                                                                   \
        unsigned int ID = RCCE_ue();                                    \
        unsigned int NUM_UES = RCCE_num_ues();                          \
        tm_init(argc, argv, ID);


    int color(int id, void *aux) {
        return !(id % DSLNDPERNODES);
    }
    
    inline void tm_init(int argc, char **argv, unsigned int ID) {
        if (ID % DSLNDPERNODES == 0) {
            //dsl node
            PRINTD("[DSL NODE]");
            dsl_init();
        }
        else { //app node
            PRINTD("[APP NODE]");
            ps_init_();
            stm_tx_node = tx_metadata_node_new();
            stm_tx = tx_metadata_new(IDLE);
            if (stm_tx == NULL || stm_tx_node == NULL) {
                PRINTD("Could not alloc tx metadata @ TM_INIT");
                EXIT(-1);
            }
        }
    }

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
        PRINTD("|| restarting due to %s", conflict_reasons[reason]);    \
        stm_tx->write_set = write_set_new();                            \
        stm_tx->read_set = read_set_new();                              \
        if (stm_tx->write_set == NULL || stm_tx->read_set == NULL) {    \
            PRINTD("Could not alloc r/w sets @ TX_START");              \
            PRINTD("  | FAKE: freeing memory");                         \
            EXIT(-1);                                                   \
        }                                                               \
    }                                                                   \
    stm_tx->retries++;                                                  \
    stm_tx->state = RUNNING;                                            \
    stm_tx->max_retries = (stm_tx->max_retries >= stm_tx->retries)      \
        ? stm_tx->max_retries : stm_tx->retries;                        


#define TX_ABORT(reason)                                \
    PRINTD("|| aborting tx");                           \
    handle_abort(stm_tx, reason);                       \
    siglongjmp(stm_tx->env, reason);

    //ps_hashtable_print(ps_hashtable);                   \
    
#define TX_COMMIT                                       \
    PRINTD("|| commiting tx");                          \
    ps_publish_all();                                   \
    write_set_persist(stm_tx->write_set);               \
    ps_finish_all();                                    \
    stm_tx->state = COMMITED;                           \
    stm_tx_node->tx_starts += stm_tx->retries;          \
    stm_tx_node->tx_commited++;                         \
    stm_tx_node->tx_aborted += stm_tx->aborts;          \
    stm_tx_node->max_retries =                          \
        (stm_tx->max_retries < stm_tx_node->max_retries)\
            ? stm_tx_node->max_retries                  \
            : stm_tx->max_retries;                      \
    stm_tx_node->aborts_war += stm_tx->aborts_war;      \
    stm_tx_node->aborts_raw += stm_tx->aborts_raw;      \
    stm_tx_node->aborts_waw += stm_tx->aborts_waw;      \
    tx_metadata_free(&stm_tx); }


#define TM_END                                          \
    PRINTD("|| FAKE: TM ends");                         \
    tx_metadata_node_print(stm_tx_node);                \
    free(stm_tx_node); }                                  


#define TX_LOAD(addr)                                   \
    tx_load(stm_tx->write_set, stm_tx->read_set, ((void *) (addr)))

#define TX_STORE(addr, ptr, datatype)                   \
    write_set_update(stm_tx->write_set, datatype, ((void *) (ptr)), ((void *) (addr)))

    inline void udelay(unsigned int micros) {
        double __ts_end = RCCE_wtime() + ((double) micros / 1000000);
        while (RCCE_wtime() < __ts_end);
    }

    inline void ps_unsubscribe_all();

    inline void handle_abort(stm_tx_t *stm_tx, CONFLICT_TYPE reason) {
        ps_finish_all();
        stm_tx->state = ABORTED;
        stm_tx->aborts++;
        switch (reason) {
            case READ_AFTER_WRITE:
                stm_tx->aborts_raw++;
                break;
            case WRITE_AFTER_READ:
                stm_tx->aborts_war++;
                break;
            case WRITE_AFTER_WRITE:
                stm_tx->aborts_waw++;
        }
        //PRINTD("  | read/write_set_free");
        write_set_free(stm_tx->write_set);
        read_set_free(stm_tx->read_set);
    }

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

    inline void ps_publish_finish_all(unsigned int locked) {
        locked = (locked != 0) ? locked : stm_tx->write_set->nb_entries;
        write_entry_t *we_current = stm_tx->write_set->write_entries;
        while (locked-- > 0) {
            ps_publish_finish((void *) we_current[locked].address_shmem);
        }
    }

    inline void ps_publish_all() {
        unsigned int locked = 0;
        write_entry_t *write_entries = stm_tx->write_set->write_entries;
        unsigned int nb_entries = stm_tx->write_set->nb_entries;
        while (locked < nb_entries) {
            CONFLICT_TYPE conflict;
#ifdef BACKOFF
            unsigned int num_delays = 0;
            unsigned int delay = BACKOFF_DELAY; //micro
retry:
#endif
            if ((conflict = ps_publish((void *) write_entries[locked].address_shmem)) != NO_CONFLICT) {
                //ps_publish_finish_all(locked);
#ifdef BACKOFF
                if (num_delays++ < BACKOFF_MAX) {
                    udelay(delay);
                    delay *= 2;
                    goto retry;
                }
#endif
                TX_ABORT(conflict);
            }
            locked++;
        }
    }

    inline void ps_unsubscribe_all() {
        read_entry_l_t *read_entries = stm_tx->read_set->read_entries;
        int i;
        for (i = 0; i < stm_tx->read_set->nb_entries; i++) {
            ps_unsubscribe((void *) read_entries[i].address_shmem);
        }
    }

#ifdef	__cplusplus
}
#endif

#endif	/* TM_H */

