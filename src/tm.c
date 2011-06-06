/*
 * File:   tm.h
 * Author: trigonak
 *
 * Created on April 13, 2011, 9:58 AM
 */

#include "tm.h"

    inline void handle_abort(stm_tx_t *stm_tx, CONFLICT_TYPE reason) {
        //PRINTD("|| handling abort");
        //PRINTD("  | release R/W locks");
        //ps_unsubscribe_all();
        //double ts = RCCE_wtime();
        ps_finish_all();
        //double te = RCCE_wtime();
        //PRINTD("/\\/\\/\\/\\ ps_finish_all took %f secs", te - ts);
        taskudelay(10); //TODO: check if needed and how it is needed
        //PRINTD("  | update tx metadata");
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
        } else {
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
                        taskudelay(delay);
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
                    taskudelay(delay);
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



