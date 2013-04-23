/*
 * File:   rw_entry_ssht.h
 *
 * Data structures to be used for the DS locking / version supporting up to 64 readers
 */


#ifndef RW_ENTRY_SSHT_H
#define	RW_ENTRY_SSHT_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "common.h"
#if !defined(NOCM) && !defined(BACKOFF_RETRY) /* if any other CM (greedy, wholly, faircm) */
#include "cm.h"
#endif

  /*__________________________________________________________________________________________________
   * RW ENTRY                                                                                         |
   * _________________________________________________________________________________________________|
   */

  /* #define NO_WRITERI 0x10000000 */
  /* #define NO_WRITERLL 0x1000000000000000ULL */
  /* #define NO_WRITER 0x1000 */

#define NO_READERS 0x0
#define NO_WRITERC 0xFF

  typedef struct ssht_rw_entry 
  {
    uint64_t readers;
    uint8_t writer;
  } ssht_rw_entry_t;

  /*___________________________________________________________________________________________________
   * Functions
   *___________________________________________________________________________________________________
   */

  INLINED BOOLEAN rw_entry_ssht_is_member(ssht_rw_entry_t *rwe, nodeid_t nodeId);

  INLINED void rw_entry_ssht_set(ssht_rw_entry_t *rwe, nodeid_t nodeId);

  INLINED void rw_entry_ssht_unset(ssht_rw_entry_t *rwe, nodeid_t nodeId);

  INLINED void rw_entry_ssht_clear(ssht_rw_entry_t *rwe);

  INLINED BOOLEAN rw_entry_ssht_is_empty(ssht_rw_entry_t *rwe);

  INLINED BOOLEAN rw_entry_ssht_has_readers(ssht_rw_entry_t *rwe);

  INLINED BOOLEAN rw_entry_ssht_is_unique_reader(ssht_rw_entry_t *rwe, nodeid_t nodeId);

  INLINED void rw_entry_ssht_set_writer(ssht_rw_entry_t *rwe, nodeid_t nodeId);

  INLINED void rw_entry_ssht_unset_writer(ssht_rw_entry_t *rwe);

  INLINED BOOLEAN rw_entry_ssht_has_writer(ssht_rw_entry_t *rwe);

  INLINED BOOLEAN rw_entry_ssht_is_writer(ssht_rw_entry_t *rwe, nodeid_t nodeId);

  INLINED ssht_rw_entry_t* rw_entry_ssht_new();

  INLINED CONFLICT_TYPE rw_entry_ssht_is_conflicting(ssht_rw_entry_t* rw_entry, nodeid_t nodeId, RW rw);

  INLINED void rw_entry_ssht_print_readers(ssht_rw_entry_t *rwe);

  /*___________________________________________________________________________________________________
   * Implementation
   *___________________________________________________________________________________________________
   */

  INLINED BOOLEAN
  rw_entry_ssht_is_member(ssht_rw_entry_t *rwe, nodeid_t nodeId)
  {
    return (BOOLEAN) ((rwe->readers >> nodeId) & 0x1);
  }

  INLINED void
  rw_entry_ssht_set(ssht_rw_entry_t *rwe, nodeid_t nodeId)
  {
    rwe->readers |= (1LL << nodeId);
  }

  INLINED void
  rw_entry_ssht_unset(ssht_rw_entry_t *rwe, nodeid_t nodeId)
  {
    rwe->readers &= ~(1LL << nodeId);
  }

  INLINED void
  rw_entry_ssht_clear(ssht_rw_entry_t *rwe)
  {
    rwe->readers = NO_READERS;
    rwe->writer = NO_WRITERC;
  }

  INLINED BOOLEAN
  rw_entry_ssht_is_empty(ssht_rw_entry_t *rwe)
  {
    return (BOOLEAN) (rwe->readers == NO_READERS && rwe->writer == NO_WRITERC);
  }

  INLINED BOOLEAN
  rw_entry_ssht_has_readers(ssht_rw_entry_t *rwe)
  {
    return (BOOLEAN) (rwe->readers != NO_READERS);
  }


  INLINED BOOLEAN
  rw_entry_ssht_is_unique_reader(ssht_rw_entry_t *rwe, nodeid_t nodeId)
  {
    ssht_rw_entry_t tmp;
    tmp.readers = NO_READERS;
    rw_entry_ssht_set(&tmp, nodeId);
    return (BOOLEAN) (tmp.readers == rwe->readers);
  }

  INLINED
  void rw_entry_ssht_fetch_readers(ssht_rw_entry_t *rwe, unsigned short *readers) 
  {
    uint64_t convert = rwe->readers;

    int i;
    for (i = 0; i < NUM_UES; i++) 
      {
	if (convert & 0x01) 
	  {
	    readers[i] = 1;
	  }
	else 
	  {
	    readers[i] = 0;
	  }
	convert >>= 1;
      }
  }

  INLINED void
  rw_entry_ssht_set_writer(ssht_rw_entry_t *rwe, nodeid_t nodeId)
  {
    rwe->writer = nodeId;
  }

  INLINED void
  rw_entry_ssht_unset_writer(ssht_rw_entry_t *rwe)
  {
    rwe->writer = NO_WRITERC;
  }

  INLINED BOOLEAN
  rw_entry_ssht_has_writer(ssht_rw_entry_t *rwe)
  {
    return (BOOLEAN) (rwe->writer != NO_WRITERC);
  }

  INLINED BOOLEAN
  rw_entry_ssht_is_writer(ssht_rw_entry_t *rwe, nodeid_t nodeId)
  {
    return (BOOLEAN) (rwe->writer == nodeId);
  }

  INLINED ssht_rw_entry_t*
  rw_entry_ssht_new()
  {
    ssht_rw_entry_t *r = (ssht_rw_entry_t *) malloc(sizeof(ssht_rw_entry_t));
    r->readers = NO_READERS;
    r->writer = NO_WRITERC;
    return r;
  }

  /*
   * Detect whether the new insert into the entry would cause a possible
   * READ/WRITE, WRITE/READ, and WRITE/WRITE conflict.
   */
  INLINED CONFLICT_TYPE
  rw_entry_ssht_is_conflicting(ssht_rw_entry_t* rw_entry, nodeid_t nodeId, RW rw) 
  {
    if (rw == WRITE) /*publishing*/
      { 
	if (rw_entry_ssht_has_writer(rw_entry)) 
	  { /*WRITE/WRITE conflict*/
	    //here the logic for WRITE -> WRITE
#if !defined(NOCM) && !defined(BACKOFF_RETRY) /* if any other CM (greedy, wholly, faircm) */
	    if (contention_manager(nodeId, &rw_entry->shorts[3], WRITE_AFTER_WRITE) == TRUE) 
	      {
		//attacker won - old aborted
		return NO_CONFLICT;
	      } 
	    else {
	      return WRITE_AFTER_WRITE;
	    }
#else
	    return WRITE_AFTER_WRITE;
#endif	/* NOCM */
	  } 
	else if (rw_entry_ssht_has_readers(rw_entry)) 
	  { /*Possible WRITE AFTER READ*/
	    /*if the only writer is the one that "asks"*/
	    if (rw_entry_ssht_is_unique_reader(rw_entry, nodeId)) 
	      {
		return NO_CONFLICT;
	      }
	    /* else {  */
	    /*WRITE AFTER READ conflict*/
	    // here the logic for READ -> WRITE
#if !defined(NOCM) && !defined(BACKOFF_RETRY) /* if any other CM (greedy, wholly, faircm) */
	    unsigned short readers[NUM_UES];
	    rw_entry_ssht_fetch_readers(rw_entry, readers);
	    readers[nodeId] = 0;
                    
	    if (contention_manager(nodeId, readers, WRITE_AFTER_READ) == TRUE) 
	      {
		rw_entry_ssht_clear(rw_entry);
		return NO_CONFLICT;
	      } 
	    else 
	      {
		return WRITE_AFTER_READ;
	      }
#else
	    return WRITE_AFTER_READ;
#endif	/* NOCM */
	/* } */
	  } 
	else 
	  {
	    return NO_CONFLICT;
	  }
      } 
    /*READING/subscribing ---------------------------------------*/
    else /*subscribing*/
      { 
	if (rw_entry_ssht_has_writer(rw_entry)
	    && !rw_entry_ssht_is_writer(rw_entry, nodeId)) /*READ AFTER WRITE*/
	  {
	    //TODO: here the logic for READ -> WRITE

#if !defined(NOCM) && !defined(BACKOFF_RETRY) /* if any other CM (greedy, wholly, faircm) */
	    if (contention_manager(nodeId, &rw_entry->shorts[3], READ_AFTER_WRITE) == TRUE) 
	      {
		//attacker won - old aborted
		rw_entry_ssht_unset_writer(rw_entry);
		return NO_CONFLICT;
	      } 
	    else 
	      {
		return READ_AFTER_WRITE;
	      }
#else
	    return READ_AFTER_WRITE;
#endif	/* NOCM */
	  } 
	else 
	  {
	    return NO_CONFLICT;
	  }
      }
  }

  INLINED void 
  rw_entry_ssht_print_readers(ssht_rw_entry_t *rwe) 
  {
    uint64_t convert = rwe->readers;
    int i;
    for (i = 0; i < 64; i++) {
      if (convert & 0x01) {
	PRINTS("%d -> ", i);
      }
      convert >>= 1;
    }

    PRINTS("NULL\n");
    FLUSH;
  }

#ifdef	__cplusplus
}
#endif

#endif	/* RW_ENTRY_SSHT_H */
