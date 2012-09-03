#ifndef _HT_H_
#define _HT_H_

#include <sys/time.h>
#include <inttypes.h>

#include "ssht_log.h"

#ifndef INLINED
#if __GNUC__ && !__GNUC_STDC_INLINE__
#define INLINED static inline __attribute__((always_inline))
#else
#define INLINED inline
#endif
#endif

#define CACHE_LINE_SIZE 64
#define SIZE_ENTRY 4
#define ADDR_PER_CL 8
#define ENTRY_PER_CL ADDR_PER_CL
#define UNUSED_DW (13 - ADDR_PER_CL)

#define MAX_READERS 62
  
#define SSHT_NO_WRITER 0xFF

#define FALSE 0
#define TRUE 1


#define NUM_BUCKETS 47

extern unsigned int num_moves;
extern unsigned int cur_size, max_size;

typedef uintptr_t addr_t;

typedef struct ssht_rw_entry {
  uint8_t nr;
  uint8_t reader[MAX_READERS];
  uint8_t writer;
} ssht_rw_entry_t;

typedef struct entry {
  union {
    struct {
      unsigned short num_readers;
      unsigned short writer;
    };
    unsigned int whole;
  };
} entry_t;

typedef struct __attribute__ ((aligned (64))) bucket {
  addr_t addr[ADDR_PER_CL];
  uint64_t num_elems;
  struct bucket * next;
  int64_t free;
  int64_t dummy[UNUSED_DW];
  ssht_rw_entry_t entry[ENTRY_PER_CL];
} bucket_t;



typedef bucket_t * ssht_hashtable_t;

INLINED ssht_hashtable_t ssht_new() {
  ssht_hashtable_t hashtable;
  hashtable = (ssht_hashtable_t) calloc(NUM_BUCKETS, sizeof(bucket_t));
  assert(hashtable != NULL);
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
    if (btmp->num_elems > 0) {
      for (i = 0; i < ADDR_PER_CL; i++) {
	if (btmp->addr[i] == addr) {
	  if (btmp->entry[i].writer == SSHT_NO_WRITER) {
	    btmp->entry[i].nr++;
	    btmp->entry[i].reader[id] = 1;
	    ssht_log_set_insert(log, btmp, i);
	    return NO_CONFLICT;
	  }
	  else {
	    return READ_AFTER_WRITE;
	  }
	}
      }
    }

    //    cur_size++;

    if (btmp->free > 0) {
      i = btmp->free;
      btmp->free = -1;
    }
    else {
      i = 0;
    }

    for (; i < ADDR_PER_CL; i++) {
      if (btmp->addr[i] == 0) {
	btmp->num_elems++;
	btmp->addr[i] = addr;
	btmp->entry[i].nr++;
	btmp->entry[i].reader[id] = 1;
	ssht_log_set_insert(log, btmp, i);
	return NO_CONFLICT;
      }
    }

    if (btmp->next == NULL) {
      btmp->next = ssht_bucket_new();
    }

    btmp = btmp->next;
  } while (1);
  
  return NO_CONFLICT; //avoid warning

}

INLINED CONFLICT_TYPE bucket_insert_w(bucket_t * bu, ssht_log_set_t *log, uint32_t id, uintptr_t addr) {
  uint32_t i;
  bucket_t *btmp = bu;
  do {
    if (btmp->num_elems > 0) {
      for (i = 0; i < ADDR_PER_CL; i++) {
	if (btmp->addr[i] == addr) {
	  if (btmp->entry[i].nr > 0) {
	    return WRITE_AFTER_READ;
	  }
	  else {
	    return WRITE_AFTER_WRITE;
	  }
	}
      }
    }

    //    cur_size++;

    if (btmp->free > 0) {
      i = btmp->free;
      btmp->free = -1;
    }
    else {
      i = 0;
    }

    for (; i < ADDR_PER_CL; i++) {
      if (btmp->addr[i] == 0) {
	btmp->num_elems++;
	btmp->addr[i] = addr;
	btmp->entry[i].writer = id;
	ssht_log_set_insert(log, btmp, i);
	return NO_CONFLICT;
      }
    }

    if (btmp->next == NULL) {
      btmp->next = ssht_bucket_new();
    }

    btmp = btmp->next;
  } while (1);
  
  return NO_CONFLICT; //avoid warning
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


INLINED uint32_t bucket_remove(bucket_t * bu, uint32_t id, uintptr_t addr) {
  uint32_t i;
  bucket_t *btmp = bu, *blastprev = bu;
  do {
    for (i = 0; i < ADDR_PER_CL; i++) {
      if (btmp->addr[i] == addr) {
	if (btmp->entry[i].writer != SSHT_NO_WRITER) {
	  btmp->entry[i].writer = SSHT_NO_WRITER;
	}
	else {
	  btmp->entry[i].nr--;
	  btmp->entry[i].reader[id] = 0;
	}

	if (btmp->entry[i].nr == 0 && btmp->entry[i].writer == SSHT_NO_WRITER) {
	  btmp->num_elems--;
	  bucket_t * blast = btmp;
	
	  if (blast->num_elems == 0 && blast != blastprev) {
	    free(blast);
	    blastprev->next = NULL;
	  }
	  else {
	    blast->free = i;
	  }
	}
	return TRUE;
      }
    }

    blastprev = btmp;
    btmp = btmp->next;
  } while (btmp != NULL);

  return FALSE;
}


INLINED uint32_t ssht_bucket_remove_index(bucket_t * bu, uint32_t id, uint32_t index) {
  if (bu->entry[index].writer != SSHT_NO_WRITER) {
    bu->entry[index].writer = SSHT_NO_WRITER;
  }
  else {
    bu->entry[index].nr--;
    bu->entry[index].reader[id] = 0;
  }
  
  if (bu->entry[index].nr == 0 && bu->entry[index].writer == SSHT_NO_WRITER) {
    bu->addr[index] = 0;
    bu->num_elems--;
    bu->free = index;
  }

  return TRUE;
}

INLINED uint32_t ssht_remove(ssht_hashtable_t ht, entry_addr_t addr, uint32_t id, uint32_t index) {
  return ssht_bucket_remove_index(addr, id, index);
}


INLINED void ht_print(bucket_t * ht) {
  unsigned int i;
  for (i = 0; i < NUM_BUCKETS; i++) {
    printf("[%02d]: ", i);
    bucket_print(ht + i);
  }
}

#endif /* _HT_H_ */
