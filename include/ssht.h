#ifndef _HT_H_
#define _HT_H_

#include <sys/time.h>
#include <inttypes.h>

#include "ssht_log.h"
#include <assert.h>
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
#  include "cm.h"
#endif 

#ifndef INLINED
#  if __GNUC__ && !__GNUC_STDC_INLINE__
#    define INLINED static inline __attribute__((always_inline))
#  else
#    define INLINED inline
#  endif
#endif

#define CACHE_LINE_SIZE 64
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


#define NUM_BUCKETS NUM_OF_BUCKETS

typedef uintptr_t addr_t;

typedef struct ssht_rw_entry {
  uint8_t nr;
  uint8_t reader[MAX_READERS];
  uint8_t writer;
} ssht_rw_entry_t;

typedef struct ALIGNED(64) bucket {          /* SCC */
  addr_t addr[ADDR_PER_CL];	             /* 4 * 7   28 */
  struct bucket * next;			     /* 4       32 */
  uint8_t padding[PADDING_BYTES];
  ssht_rw_entry_t entry[ENTRY_PER_CL];
} bucket_t;



typedef bucket_t * ssht_hashtable_t;

INLINED ssht_hashtable_t ssht_new() {
  ssht_hashtable_t hashtable;
  hashtable = (ssht_hashtable_t) calloc(NUM_BUCKETS, sizeof(bucket_t));
  assert(hashtable != NULL);

  assert(sizeof(ssht_rw_entry_t) % CACHE_LINE_SIZE == 0);
  assert(sizeof(bucket_t) % CACHE_LINE_SIZE == 0);

  unsigned int i;
  for (i = 0; i < NUM_BUCKETS; i++) {
    unsigned int j;
    for (j = 0; j < ENTRY_PER_CL; j++) {
      hashtable[i].entry[j].writer = SSHT_NO_WRITER;
    }
  }

  return hashtable;
}

INLINED bucket_t * ssht_bucket_new() {
  bucket_t * bu = (bucket_t *) calloc(1, sizeof(bucket_t));
  assert(bu != NULL);

  uint32_t i;
  for (i = 0; i < ENTRY_PER_CL; i++) {
    bu->entry[i].writer = SSHT_NO_WRITER;
  }

  return bu;
}

INLINED void bucket_print(bucket_t * bu) {
  bucket_t * btmp = bu;
  do {
    unsigned int j;
    for (j = 0; j < ADDR_PER_CL; j++) {
      printf("%p:%2d/%d|", (void *)btmp->addr[j], btmp->entry[j].nr, btmp->entry[j].writer);
    }
    btmp = btmp->next;
    printf("|");
  } while (btmp != NULL);
  printf("\n");
}


INLINED CONFLICT_TYPE bucket_insert_r(bucket_t * bu, ssht_log_set_t *log, uint32_t id, uintptr_t addr) {
  uint32_t i;
  bucket_t *btmp = bu;
  do {
      for (i = 0; i < ADDR_PER_CL; i++) {
	if (btmp->addr[i] == addr) {
	  ssht_rw_entry_t *e = btmp->entry + i;
	  if (e->writer != SSHT_NO_WRITER) {
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
	    if (!contention_manager_raw_waw(id, (uint16_t) e->writer, READ_AFTER_WRITE)) {
	      return READ_AFTER_WRITE;
	    }
#else
	    return READ_AFTER_WRITE;
#endif	/* NOCM */
	  }

	  e->nr++;
	  e->reader[id] = 1;
	  btmp->addr[i] = addr;
	  ssht_log_set_insert(log, btmp->addr + i, e);
	  return NO_CONFLICT;
	}
      }

    btmp = btmp->next;
  } while (btmp != NULL);

  btmp = bu;

  do {
    for (i = 0; i < ADDR_PER_CL; i++) {
      if (btmp->addr[i] == 0) {
	ssht_rw_entry_t *e = btmp->entry + i;
	e->nr++;
	e->reader[id] = 1;
	btmp->addr[i] = addr;
	ssht_log_set_insert(log, btmp->addr + i, e);
	return NO_CONFLICT;
      }
    }

    if (btmp->next == NULL) {
      btmp->next = ssht_bucket_new();
    }

    btmp = btmp->next;
  } while (1);
}  

INLINED uint32_t ssht_rw_entry_has_readers(ssht_rw_entry_t * entry) {
  return entry->nr;
}

INLINED CONFLICT_TYPE bucket_insert_w(bucket_t * bu, ssht_log_set_t *log, uint32_t id, uintptr_t addr) {
  uint32_t i;
  bucket_t *btmp = bu;
  do {
    for (i = 0; i < ADDR_PER_CL; i++) {
      if (btmp->addr[i] == addr) {
	  ssht_rw_entry_t *e = btmp->entry + i;
	  if (e->writer != SSHT_NO_WRITER) {
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
	  if (contention_manager_raw_waw(id, e->writer, WRITE_AFTER_WRITE)) {
	    e->writer = id;
	    btmp->addr[i] = addr;
	    ssht_log_set_insert(log, btmp->addr + i, e);
	    return NO_CONFLICT;
	  }
	  else {
	    return WRITE_AFTER_WRITE;
	  }
#else
	  return WRITE_AFTER_WRITE;
#endif	/* NOCM */
	}
	else {
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
	  if (contention_manager_war(id, e->reader, WRITE_AFTER_READ)) {
	    e->writer = id;
	    btmp->addr[i] = addr;
	    ssht_log_set_insert(log, btmp->addr + i, e);
	    return NO_CONFLICT;
	  }
	  else {
	    return WRITE_AFTER_READ;
	  }
#else
	  return WRITE_AFTER_READ;
#endif	/* NOCM */
	}
      }
    }
    
    btmp = btmp->next;
  } while (btmp != NULL);

  btmp = bu;
  do {
    for (i = 0; i < ADDR_PER_CL; i++) {
      if (btmp->addr[i] == 0) {
	ssht_rw_entry_t *e = btmp->entry + i;
	e->writer = id;
	btmp->addr[i] = addr;
	ssht_log_set_insert(log, btmp->addr + i, e);
	return NO_CONFLICT;
      }
    }

    if (btmp->next == NULL) {
      btmp->next = ssht_bucket_new();
    }

    btmp = btmp->next;
  } while (1);
}


INLINED CONFLICT_TYPE ssht_insert(ssht_hashtable_t ht, uint32_t bu, ssht_log_set_t * log, uint32_t id, uintptr_t addr, RW rw) {
  CONFLICT_TYPE ct;
  if (rw == READ) {
    ct = bucket_insert_r(ht + bu, log, id, addr);
  }
  else {
    ct = bucket_insert_w(ht + bu, log, id, addr);
  }

  return ct;
}


INLINED uint32_t ssht_bucket_remove_index(addr_t *addr, uint32_t id, ssht_rw_entry_t *entry) {
  if (entry->writer == id) {
    entry->writer = SSHT_NO_WRITER;
  }
  else {
    entry->reader[id] = 0;
    entry->nr--;
  }
    
  if (entry->nr == 0 && entry->writer == SSHT_NO_WRITER) {
    *addr = 0;
  }

  return TRUE;
}

INLINED uint32_t ssht_remove(addr_t *addr, uint32_t id, ssht_rw_entry_t *entry) {
  return ssht_bucket_remove_index(addr, id, entry);
}



INLINED void ht_print(bucket_t * ht) {
  unsigned int i;
  for (i = 0; i < NUM_BUCKETS; i++) {
    printf("[%02d]: ", i);
    bucket_print(ht + i);
  }
}

#endif /* _HT_H_ */
