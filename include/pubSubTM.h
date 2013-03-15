/* 
 * File:   pubSubTM.h
 * Author: trigonak
 *
 * Created on March 7, 2011, 10:50 AM
 * 
 * The main DTM system functionality
 * 
 * Subscribe == read (read-lock)
 * Publish == write (write-lock)
 */

#ifndef PUBSUBTM_H
#define	PUBSUBTM_H

#include "common.h"
#include "stm.h"
#include "messaging.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define RESP_NODE_MASK 6

  //the buffer size for pub-sub = the size (in bytes) of the msg exchanged
#define PS_BUFFER_SIZE     32

  //the number of operators that pub-sub supports
#define PS_NUM_OPS         6
  //pub-sub request types

  //TODO: remove ? have them at .c file
  extern int64_t read_value;
  extern nodeid_t* dsl_nodes;
  extern unsigned long int* tm2c_rand_seeds;

  //void ps_init(void);
  void ps_init_(void);

  /*

    TODO: move the inline function implementations here..

  */

  /* Try to subscribe the TX for reading the address
   */
  CONFLICT_TYPE ps_subscribe(tm_addr_t address, int words);

  /* Try to publish a write on the address
   * XXX: try to unify the interface
   */
#ifdef PGAS
  CONFLICT_TYPE ps_publish(tm_addr_t address, int64_t value);

  /*  Try to increment by increment and store (so, write) address
   */
  CONFLICT_TYPE ps_store_inc(tm_addr_t address, int64_t increment);
#else
  CONFLICT_TYPE ps_publish(tm_addr_t address);
#endif

  /* Non-transactional read of 1 or 2 words from an address */
  uint64_t ps_load(tm_addr_t address, int words);

  /* Non-transactional write to an address */
  void ps_store(tm_addr_t address, int64_t value);

  /* Unsubscribes the TX from the address
   */
  void ps_unsubscribe(tm_addr_t address);
  /* Notifies the pub-sub that the publishing is done, so the value
   * is written on the shared mem
   */
  void ps_publish_finish(tm_addr_t address);

  void ps_finish_all(CONFLICT_TYPE conflict);

  void ps_send_stats(stm_tx_node_t *, double duration);
#ifdef	__cplusplus
}
#endif

#endif	/* PUBSUBTM_H */
