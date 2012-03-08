/* 
 * File:   tm.c
 * Author: trigonak
 *
 * Created on June 16, 2011, 12:29 PM
 */

#include <sys/param.h>

#include "tm.h"
#ifdef PGAS
#include "pgas.h"
#endif

nodeid_t ID; //=RCCE_ue()
nodeid_t NUM_UES;
nodeid_t NUM_DSL_NODES;
nodeid_t NUM_APP_NODES;

stm_tx_t *stm_tx = NULL;
stm_tx_node_t *stm_tx_node = NULL;

double duration__ = 0;

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

void tm_init(nodeid_t ID) {
	/* initialize globals */
	ID            = NODE_ID();
	NUM_UES       = TOTAL_NODES();
	NUM_DSL_NODES = ((NUM_UES/DSLNDPERNODES)) + (NUM_UES%DSLNDPERNODES ? 1 : 0);
	NUM_APP_NODES = NUM_UES-NUM_DSL_NODES;

    sys_tm_init(ID);

    if (ID % DSLNDPERNODES == 0) {
        //dsl node
        dsl_init();
    }
    else { //app node
        ps_init_();
        stm_tx_node = tx_metadata_node_new();
        stm_tx = tx_metadata_new(IDLE);
        if (stm_tx == NULL || stm_tx_node == NULL) {
            PRINTD("Could not alloc tx metadata @ TM_INIT");
            EXIT(-1);
        }

#ifdef PGAS
        PGAS_alloc_init(0);
#endif
    }
}

/*
 * Trampolining code for terminating everything
 */
void
tm_term()
{
    if (ID % DSLNDPERNODES == 0) {
        // DSL node
        // common stuff

        // platform specific stuff
        sys_dsl_term();
    }
    else { 
    	//app node
        // common stuff

        // plaftom specific stuff
        sys_ps_term();
    }
}

void handle_abort(stm_tx_t *stm_tx, CONFLICT_TYPE reason) {
    ps_finish_all(reason);
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
#ifdef PGAS
    write_set_pgas_empty(stm_tx->write_set);
#else
    write_set_empty(stm_tx->write_set);
#endif
    read_set_empty(stm_tx->read_set);
    mem_info_on_abort(stm_tx->mem_info);
    
#ifndef BACKOFF
    /*BACKOFF and RETRY*/
    unsigned int wait_max = pow(2, (stm_tx->retries < BACKOFF_MAX ? stm_tx->retries : BACKOFF_MAX)) * BACKOFF_DELAY;
    unsigned int wait = rand_range(wait_max);
    //PRINT("\t\t\t\t\t\t... backoff for %5d micros (retries: %3d | max: %d)", wait, stm_tx->retries, wait_max);
    udelay(wait);
#endif
    
}

void ps_publish_finish_all(unsigned int locked) {
    locked = (locked != 0) ? locked : stm_tx->write_set->nb_entries;
#ifdef PGAS
    write_entry_pgas_t *we_current = stm_tx->write_set->write_entries;
#else
    write_entry_t *we_current = stm_tx->write_set->write_entries;
#endif
    while (locked-- > 0) {
#ifdef PGAS
        // ps_publish_finish(we_current->address, we_current->value);
#else
        ps_publish_finish((void *) we_current[locked].address);
#endif
    }
}

void ps_publish_all() {
    unsigned int locked = 0;
#ifdef PGAS
    write_entry_pgas_t *write_entries = stm_tx->write_set->write_entries;
#else
    write_entry_t *write_entries = stm_tx->write_set->write_entries;
#endif
    unsigned int nb_entries = stm_tx->write_set->nb_entries;
    while (locked < nb_entries) {
        CONFLICT_TYPE conflict;
#ifdef BACKOFF
        unsigned int num_delays = 1;
        unsigned int delay = BACKOFF_DELAY; //micro
retry:
#endif
#ifdef PGAS
        if ((conflict = ps_publish((tm_addr_t)write_entries[locked].address, write_entries[locked].value)) != NO_CONFLICT) {
#else
        if ((conflict = ps_publish(write_entries[locked].address)) != NO_CONFLICT) {
#endif
            //ps_publish_finish_all(locked);
#ifdef BACKOFF
            if (num_delays++ < BACKOFF_MAX) {
                udelay(delay);
                delay = (2^num_delays) * BACKOFF_DELAY;
                goto retry;
            }
#endif
            TX_ABORT(conflict);
        }
        locked++;
    }
}

void ps_unsubscribe_all() {
    read_entry_l_t *read_entries = stm_tx->read_set->read_entries;
    unsigned int i;
    for (i = 0; i < stm_tx->read_set->nb_entries; i++) {
        ps_unsubscribe((void *) read_entries[i].address);
    }
}
