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

#include <setjmp.h>
#include "log.h"
#include "mem.h"

  typedef enum 
    {
      IDLE,
      RUNNING,
      ABORTED,
      COMMITED
    } TX_STATE;

  typedef struct ALIGNED(64) stm_tx /* Transaction descriptor */
  { 
    sigjmp_buf env;		/* Environment for setjmp/longjmp */
#if defined(GREEDY) /* placed in diff place than for FAIRCM, according to access seq */
    ticks start_ts;
#eif defined(FAIRCM) 
    ticks start_ts;
#endif
    uint16_t aborts;	 /* Total number of aborts (cumulative) */
    uint16_t aborts_raw; /* Aborts due to read after write (cumulative) */
    uint16_t aborts_war; /* Aborts due to write after read (cumulative) */
    uint16_t aborts_waw; /* Aborts due to write after write (cumulative) */
    uint16_t retries;	  /* Number of consecutive aborts (retries) */
    mem_info_t* mem_info; /* Transactional mem alloc lists*/
#if !defined(PGAS)		/* in PGAS only the DSLs hold a write_set */
    write_set_t *write_set;	/* Write set */
#endif
  } stm_tx_t;

  typedef struct ALIGNED(64) stm_tx_node 
  {
    uint32_t tx_starts;
    uint32_t tx_commited;
    uint32_t tx_aborted;
    uint32_t max_retries;
    uint32_t aborts_war;
    uint32_t aborts_raw;
    uint32_t aborts_waw;
#ifdef FAIRCM
    ticks tx_duration;
#else
    uint8_t padding[36];
#endif
  } stm_tx_node_t;

  extern void tx_metadata_node_print(stm_tx_node_t * stm_tx_node);
  extern void tx_metadata_print(stm_tx_t* stm_tx);
  extern stm_tx_node_t* tx_metadata_node_new();
  extern stm_tx_t* tx_metadata_new();
  extern void tx_metadata_free(stm_tx_t **stm_tx);

  INLINED stm_tx_t* 
  tx_metadata_empty(stm_tx_t *stm_tx_temp) 
  {

#if !defined(PGAS)
    stm_tx_temp->write_set = write_set_empty(stm_tx_temp->write_set);
#endif
    //stm_tx_temp->mem_info = mem_info_new();
    //TODO: what about the env?
    stm_tx_temp->retries = 0;
    stm_tx_temp->aborts = 0;
    stm_tx_temp->aborts_war = 0;
    stm_tx_temp->aborts_raw = 0;
    stm_tx_temp->aborts_waw = 0;
    return stm_tx_temp;
  }

#ifdef	__cplusplus
}
#endif

#endif	/* STM_H */

