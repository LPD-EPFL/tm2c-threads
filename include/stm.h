/* 
 * File:   stm.h
 * Author: trigonak
 *
 * Created on April 11, 2011, 6:05 PM
 * 
 * Data structures and operations related to the TM metadata
 */

#ifndef STM_H
#define	STM_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "log.h"
#include "mem.h"

    typedef enum {
        IDLE,
        RUNNING,
        ABORTED,
        COMMITED
    } TX_STATE;

    typedef struct stm_tx { /* Transaction descriptor */
        TX_STATE state; /* Transaction status*/
        //        stm_word_t start; /* Start timestamp */
        //        stm_word_t end; /* End timestamp (validity range) */
        read_set_t *read_set; /* Read set */
        write_set_t *write_set; /* Write set */
        mem_info_t *mem_info; /* Transactional mem alloc lists*/
        sigjmp_buf env; /* Environment for setjmp/longjmp */
        sigjmp_buf *jmp; /* Pointer to environment (NULL when not using setjmp/longjmp) */
        //        int nesting; /* Nesting level */
        //        int ro; /* Is this execution read-only? */
        unsigned long retries; /* Number of consecutive aborts (retries) */
        unsigned long aborts; /* Total number of aborts (cumulative) */
        //unsigned long aborts_ro; /* Aborts due to wrong read-only specification (cumulative) */
        unsigned long aborts_war; /* Aborts due to write after read (cumulative) */
        unsigned long aborts_raw; /* Aborts due to read after write (cumulative) */
        unsigned long aborts_waw; /* Aborts due to write after write (cumulative) */
        unsigned long max_retries; /* Maximum number of consecutive aborts (retries) */
    } stm_tx_t;

    typedef struct stm_tx_node {
        unsigned long tx_starts;
        unsigned long tx_commited;
        unsigned long tx_aborted;
        unsigned long max_retries;
        unsigned long aborts_war;
        unsigned long aborts_raw;
        unsigned long aborts_waw;
    } stm_tx_node_t;

    stm_tx_t *stm_tx = NULL;
    stm_tx_node_t *stm_tx_node = NULL;

    inline void tx_metadata_node_print(stm_tx_node_t * stm_tx_node) {
        printf("TXs Statistics for node --------------------------------------\n");
        printf("Starts      \t: %lu\n", stm_tx_node->tx_starts);
        printf("Commits     \t: %lu\n", stm_tx_node->tx_commited);
        printf("Aborts      \t: %lu\n", stm_tx_node->tx_aborted);
        printf("Max Retries \t: %lu\n", stm_tx_node->max_retries);
        printf("Aborts WAR  \t: %lu\n", stm_tx_node->aborts_war);
        printf("Aborts RAW  \t: %lu\n", stm_tx_node->aborts_raw);
        printf("Aborts WAW  \t: %lu\n", stm_tx_node->aborts_waw);
        printf("--------------------------------------------------------------\n");
        fflush(stdout);
    }

    inline void tx_metadata_print(stm_tx_t * stm_tx) {
        printf("TX Statistics ------------------------------------------------\n");
        printf("Retries     \t: %lu\n", stm_tx->retries);
        printf("Aborts      \t: %lu\n", stm_tx->aborts);
        printf("Max Retries \t: %lu\n", stm_tx->max_retries);
        printf("Aborts WAR  \t: %lu\n", stm_tx->aborts_war);
        printf("Aborts RAW  \t: %lu\n", stm_tx->aborts_raw);
        printf("Aborts WAW  \t: %lu\n", stm_tx->aborts_waw);
        printf("--------------------------------------------------------------\n");
        fflush(stdout);
    }

    inline stm_tx_node_t * tx_metadata_node_new() {
        stm_tx_node_t *stm_tx_node_temp = (stm_tx_node_t *) malloc(sizeof (stm_tx_node_t));
        if (stm_tx_node_temp == NULL) {
            printf("malloc stm_tx_node @ tx_metadata_node_new");
            return NULL;
        }

        stm_tx_node_temp->tx_starts = 0;
        stm_tx_node_temp->tx_commited = 0;
        stm_tx_node_temp->tx_aborted = 0;
        stm_tx_node_temp->max_retries = 0;
        stm_tx_node_temp->aborts_war = 0;
        stm_tx_node_temp->aborts_raw = 0;
        stm_tx_node_temp->aborts_waw = 0;

        return stm_tx_node_temp;
    }

    inline stm_tx_t * tx_metadata_new(TX_STATE state) {
        stm_tx_t *stm_tx_temp = (stm_tx_t *) malloc(sizeof (stm_tx_t));
        if (stm_tx_temp == NULL) {
            printf("malloc stm_tx @ tx_metadata_new");
            return NULL;
        }

        stm_tx_temp->state = state;
        stm_tx_temp->read_set = read_set_new();
        stm_tx_temp->write_set = write_set_new();
        stm_tx_temp->mem_info = mem_info_new();
        //TODO: what about the env?
        stm_tx_temp->retries = 0;
        stm_tx_temp->aborts = 0;
        stm_tx_temp->aborts_war = 0;
        stm_tx_temp->aborts_raw = 0;
        stm_tx_temp->aborts_waw = 0;
        stm_tx_temp->max_retries = 0;

        return stm_tx_temp;
    }

    inline stm_tx_t * tx_metadata_empty(stm_tx_t *stm_tx_temp) {

        stm_tx_temp->read_set = read_set_empty(stm_tx_temp->read_set);
        stm_tx_temp->write_set = write_set_empty(stm_tx_temp->write_set);
        //stm_tx_temp->mem_info = mem_info_new();
        //TODO: what about the env?
        stm_tx_temp->retries = 0;
        stm_tx_temp->aborts = 0;
        stm_tx_temp->aborts_war = 0;
        stm_tx_temp->aborts_raw = 0;
        stm_tx_temp->aborts_waw = 0;
        stm_tx_temp->max_retries = 0;

        return stm_tx_temp;
    }

    inline void tx_metadata_free(stm_tx_t **stm_tx) {
        //TODO: "clear" insted of freeing the stm_tx
        
        write_set_free((*stm_tx)->write_set);
        read_set_free((*stm_tx)->read_set);
        mem_info_free((*stm_tx)->mem_info);
        free((*stm_tx));
        *stm_tx = NULL;
    }

#ifdef	__cplusplus
}
#endif

#endif	/* STM_H */

