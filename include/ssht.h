#ifndef _HT_H_
#define _HT_H_

#include <sys/time.h>
#include <inttypes.h>
#include <limits.h>

#include "common.h"
#include "hash.h"

#include "ssht_log.h"
#include <assert.h>
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
#  include "cm.h"
#endif 

#define SIZE_ENTRY 4
#define ADDR_PER_CL 7
#define ENTRY_PER_CL ADDR_PER_CL

#ifdef SCC
#define PADDING_BYTES 32
#else
#define PADDING_BYTES 0
#endif	/* SCC */

#define MAX_READERS 62
  
#define SSHT_NO_WRITER 0xFF
#define SSHT_ENTRY_FREE 0x00000000000000FF

#define FALSE 0
#define TRUE 1

#define NUM_OF_BUCKETS 47
#define NUM_BUCKETS NUM_OF_BUCKETS

typedef uintptr_t addr_t;

typedef struct ALIGNED(64) ssht_rw_entry {
  uint8_t nr;
  uint8_t reader[MAX_READERS];
  uint8_t writer;
} ssht_rw_entry_t;

typedef struct ALIGNED(64) bucket {          /* SCC */
  addr_t addr[ADDR_PER_CL];	             /* 4 * 7   28 */
  struct bucket* next;			     /* 4       32 */
  uint8_t padding[PADDING_BYTES];
  ssht_rw_entry_t entry[ENTRY_PER_CL];
} bucket_t;


#if defined(SSHT_DBG_UTILIZATION)
extern uint32_t ssht_dbg_bu_expansions;
extern uint32_t ssht_dbg_usages;
extern uint32_t ssht_dbg_bu_usages[NUM_BUCKETS];
extern uint32_t ssht_dbg_bu_usages_w[NUM_BUCKETS];
extern uint32_t ssht_dbg_bu_usages_r[NUM_BUCKETS];
#endif	/* SSHT_DBG_UTILIZATION */

typedef bucket_t* ssht_hashtable_t;

extern ssht_hashtable_t ssht_new();

extern void bucket_print(bucket_t* bu);
extern void ssht_stats_print(ssht_hashtable_t ht, uint32_t details);

extern CONFLICT_TYPE bucket_insert_r(bucket_t* bu, ssht_log_set_t* log, uint32_t id, uintptr_t addr); 
extern CONFLICT_TYPE bucket_insert_w(bucket_t* bu, ssht_log_set_t* log, uint32_t id, uintptr_t addr);
extern CONFLICT_TYPE ssht_insert_w_test(ssht_hashtable_t ht, uint32_t id, uintptr_t addr);


INLINED bucket_t* 
ssht_bucket_new() 
{
#if defined(SSHT_DBG_UTILIZATION)
  ssht_dbg_bu_expansions++;
#endif	/* SSHT_DBG_UTILIZATION */
  bucket_t* bu = (bucket_t *) calloc(1, sizeof(bucket_t));
  assert(bu != NULL);

  uint32_t i;
  for (i = 0; i < ENTRY_PER_CL; i++) 
    {
      bu->entry[i].writer = SSHT_NO_WRITER;
    }

  return bu;
}


#define ssht_rw_entry_has_readers(entry) (entry)->nr

INLINED CONFLICT_TYPE 
ssht_insert(ssht_hashtable_t ht, uint32_t bu, ssht_log_set_t* log, uint32_t id, uintptr_t addr, RW rw) 
{
#if defined(SSHT_DBG_UTILIZATION)
  ssht_dbg_usages++;
  ssht_dbg_bu_usages[bu]++;
#endif	/* SSHT_DBG_UTILIZATION */
  CONFLICT_TYPE ct;
  if (rw == READ) 
    {
#if defined(SSHT_DBG_UTILIZATION)
      ssht_dbg_bu_usages_r[bu]++;
#endif	/* SSHT_DBG_UTILIZATION */
      ct = bucket_insert_r(ht + bu, log, id, addr);
    }
  else 
    {
#if defined(SSHT_DBG_UTILIZATION)
      ssht_dbg_bu_usages_w[bu]++;
#endif	/* SSHT_DBG_UTILIZATION */
      ct = bucket_insert_w(ht + bu, log, id, addr);
    }

  return ct;
}




INLINED uint32_t 
ssht_bucket_remove_index(addr_t *addr, uint32_t id, ssht_rw_entry_t *entry) 
{
  if (entry->writer == id) 
    {
      entry->writer = SSHT_NO_WRITER;
    }
  else 
    {
      entry->reader[id] = 0;
      entry->nr--;
    }
    
  if (entry->nr == 0 && entry->writer == SSHT_NO_WRITER) 
    {
      *addr = 0;
    }

  return TRUE;
}

INLINED uint32_t 
ssht_remove(addr_t *addr, uint32_t id, ssht_rw_entry_t *entry) 
{
  return ssht_bucket_remove_index(addr, id, entry);
}


INLINED void 
ht_print(bucket_t * ht) 
{
  unsigned int i;
  for (i = 0; i < NUM_BUCKETS; i++) 
    {
      printf("[%02d]: ", i);
      bucket_print(ht + i);
    }
}

#endif /* _HT_H_ */
