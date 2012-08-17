#ifndef _HT_H_
#define _HT_H_

#include <inttypes.h>

#ifndef INLINED
#if __GNUC__ && !__GNUC_STDC_INLINE__
#define INLINED static inline __attribute__((always_inline))
#else
#define INLINED inline
#endif
#endif

#define CACHE_LINE_SIZE 64
#define SIZE_ENTRY 4
#define ENTRY_PER_CL 8
#define ADDR_PER_CL ENTRY_PER_CL
#define UNUSED_BYTES 16

//#define NO_WRITER 256
#define ENTRY_FREE 268435456


#define FALSE 0
#define TRUE 1

#define NUM_BUCKETS NUM_OF_BUCKETS

extern unsigned int num_moves;

typedef uintptr_t addr_t;

typedef struct entry {
  union {
    struct {
      unsigned short num_readers;
      unsigned short writer;
    };
    unsigned int whole;
  };
} entry_t;

/*
  typedef struct bucket {
  unsigned int empty;
  addr_t addr[ADDR_PER_CL];
  struct bucket * next;
  entry_t entry[ENTRY_PER_CL];
  } bucket_t;
*/

typedef struct bucket {
  addr_t addr[ADDR_PER_CL];
  uint64_t empty;
  struct bucket * next;
  entry_t entry[ENTRY_PER_CL];
  char dummy[UNUSED_BYTES];
} bucket_t;



typedef bucket_t * ssht_hashtable_t;

INLINED ssht_hashtable_t ssht_new() {
  ssht_hashtable_t hashtable;
  hashtable = (ssht_hashtable_t) calloc(NUM_BUCKETS, sizeof(bucket_t));
  assert(hashtable != NULL);

  unsigned int i;
  for (i = 0; i < NUM_BUCKETS; i++) {
    unsigned int j;
    for (j = 0; j < ENTRY_PER_CL; j++) {
      hashtable[i].entry[j].writer = NO_WRITER;
    }
  }

  return hashtable;
}

INLINED bucket_t * ssht_bucket_new() {
  bucket_t * bu = (bucket_t *) calloc(1, sizeof(bucket_t));
  assert(bu != NULL);

  uint32_t i;
  for (i = 0; i < ENTRY_PER_CL; i++) {
    bu->entry[i].writer = NO_WRITER;
  }

  return bu;
}

#define EMBED_BREAKPOINT \
  asm("0:"                              \
  ".pushsection embed-breakpoints;" \
  ".quad 0b;"                       \
      ".popsection;")

INLINED void bucket_print(bucket_t * bu) {
  bucket_t * btmp = bu;
  do {
    unsigned int j;
    for (j = 0; j < ADDR_PER_CL; j++) {
      if (btmp->entry[j].whole == ENTRY_FREE) {
	printf("..%3d*[]", ADDR_PER_CL - j);
	break;
      }
      printf("%p:%2d/%d|", (void *)btmp->addr[j], btmp->entry[j].num_readers, btmp->entry[j].writer);
    }
    btmp = btmp->next;
    printf("|");
  } while (btmp != NULL);
  printf("\n");
}

INLINED CONFLICT_TYPE ssht_bucket_insert(bucket_t * bu, uintptr_t addr, RW rw, uint32_t id) {
  uint32_t i, empty = bu->empty;
  for (i = 0; i < empty; i++) {
    if (bu->addr[i] == addr) {          /* READ */
      if (rw == READ) {
	if (bu->entry[i].writer == NO_WRITER) {
	  bu->entry[i].num_readers++;
	  return NO_CONFLICT;
	}
	else {
	  return READ_AFTER_WRITE;
	}
      }
      else {                           /* WRITE */
	if (bu->entry[i].num_readers > 0) {
	  return WRITE_AFTER_READ;
	}
	else {
	  return WRITE_AFTER_WRITE;
	}
      }
    }
  }

  if (i < ADDR_PER_CL) {
    bu->empty = empty + 1;
    bu->addr[i] = addr;
    if (rw == READ) {
      bu->entry[i].num_readers++;
    }
    else {
      bu->entry[i].writer = id;
    }
    return NO_CONFLICT;
  }
  else if (bu->next == NULL) {
    bu->next = ssht_bucket_new();
  }

  return ssht_bucket_insert(bu->next, addr, rw, id);
}


INLINED CONFLICT_TYPE bucket_insert_r(bucket_t * bu, unsigned int addr) {
  uint32_t i, empty;
  bucket_t *btmp = bu;
  do {
    empty = btmp->empty;
    for (i = 0; i < empty; i++) {
      if (btmp->addr[i] == addr) {
	if (btmp->entry[i].writer == NO_WRITER) {
	  btmp->entry[i].num_readers++;
	  return NO_CONFLICT;
	}
	else {
	  return READ_AFTER_WRITE;
	}
      }
    }

    if (i < ADDR_PER_CL) {
      btmp->empty = empty + 1;
      btmp->addr[i] = addr;
      btmp->entry[i].num_readers++;
      return NO_CONFLICT;
    }
    else if (btmp->next == NULL) {
      btmp->next = ssht_bucket_new();
    }

    btmp = btmp->next;
  } while (1);
  
  return NO_CONFLICT; //avoid warning
}

INLINED CONFLICT_TYPE bucket_insert_w(bucket_t * bu, uintptr_t addr, uint32_t id) {
  uint32_t i, empty;
  bucket_t *btmp = bu;
  do {
    empty = btmp->empty;
    for (i = 0; i < empty; i++) {
      if (btmp->addr[i] == addr) {
	if (btmp->entry[i].num_readers > 0) {
	  return WRITE_AFTER_READ;
	}
	else if (btmp->entry[i].writer != id) {
	  return WRITE_AFTER_WRITE;
	}
	else {
	  return NO_CONFLICT;
	}
      }
    }

    if (i < ADDR_PER_CL) {
      btmp->empty = empty + 1;
      btmp->addr[i] = addr;
      btmp->entry[i].writer = id;
      return NO_CONFLICT;
    }
    else if (btmp->next == NULL) {
      btmp->next = ssht_bucket_new();
    }

    btmp = btmp->next;
  } while (1);
  
  return NO_CONFLICT; //avoid warning
}


INLINED CONFLICT_TYPE ssht_insert(uint32_t id, ssht_hashtable_t ht, uint32_t bu, uintptr_t addr, RW rw) {
  if (rw == READ) {
    return bucket_insert_r(ht + bu, addr);
  }
  else {
    return bucket_insert_w(ht + bu, addr, id);
  }
}

INLINED uint32_t bucket_remove(bucket_t * bu, uintptr_t addr) {
  uint32_t i, empty;
  bucket_t *btmp = bu, *blastprev = bu;
  do {
    empty = btmp->empty;
    for (i = 0; i < empty; i++) {
      if (btmp->addr[i] == addr) {
	if (btmp->entry[i].writer != NO_WRITER) {
	  btmp->entry[i].writer = NO_WRITER;
	}
	else {
	  btmp->entry[i].num_readers--;
	}

	if (btmp->entry[i].whole == ENTRY_FREE) {
	  btmp->addr[i] = 0;
	  bucket_t * blast = btmp;
	  while (blast->next != NULL) {
	    blastprev = blast;
	    blast = blast->next;
	  }

	  uint32_t move = blast->empty - 1;
	  btmp->addr[i] = blast->addr[move];
	  btmp->entry[i].whole = blast->entry[move].whole;
	
	  blast->addr[move] = 0;
	  blast->entry[move].whole = ENTRY_FREE;
	  blast->empty--;
	
	  if (blast->empty == 0) {
	    if (blast != blastprev) {
	      free(blast);
	      blastprev->next = NULL;
	    }
	  }
	}
	return TRUE;
      }
    }

    /** if we want to garbage collect instead of free on sight:
	if (emtpy < ADDR_PER_CL) {
	return FALSE;
	}
    */

    blastprev = btmp;
    btmp = btmp->next;
  } while (btmp != NULL);
  return FALSE;
}


INLINED uint32_t ssht_removeg(ssht_hashtable_t ht, uint32_t bu, uintptr_t addr) {
  return bucket_remove(ht + bu, addr);
}


INLINED uint32_t bucket_remove_r(bucket_t * bu, uintptr_t addr) {
  uint32_t i, empty = bu->empty;
  bucket_t *btmp = bu, *blastprev = bu;
  do {
    for (i = 0; i < empty; i++) {
      if (btmp->addr[i] == addr) {
	if (--btmp->entry[i].num_readers == 0) {
	  bucket_t * blast = btmp;
	  while (blast->next != NULL) {
	    blastprev = blast;
	    blast = blast->next;
	  }

	  uint32_t move = blast->empty - 1;
	  btmp->addr[i] = blast->addr[move];
	  btmp->entry[i].whole = blast->entry[move].whole;
	
	  blast->addr[move] = 0;
	  blast->entry[move].whole = ENTRY_FREE;
	  blast->empty--;
	
	  if (blast->empty == 0) {
	    if (blast != blastprev) {
	      free(blast);
	      blastprev->next = NULL;
	    }
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


INLINED uint32_t bucket_remove_w(bucket_t * bu, uintptr_t addr) {
  uint32_t i, empty = bu->empty;
  bucket_t *btmp = bu, *blastprev = bu;
  do {
    for (i = 0; i < empty; i++) {
      if (btmp->addr[i] == addr) {
	bucket_t *blast = btmp;
	while (blast->next != NULL) {
	  blastprev = blast;
	  blast = blast->next;
	}

	uint32_t move = blast->empty - 1;
	btmp->addr[i] = blast->addr[move];
	btmp->entry[i].whole = blast->entry[move].whole;
	
	blast->addr[move] = 0;
	blast->entry[move].whole = ENTRY_FREE;
	blast->empty--;
	
	if (blast->empty == 0) {
	  if (blast != blastprev) {
	    free(blast);
	    blastprev->next = NULL;
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

INLINED unsigned int ssht_remove(ssht_hashtable_t ht, uint32_t bu, uintptr_t addr, RW rw) {
  if (rw == READ) {
    return bucket_remove_r(ht + bu, addr);
  }
  else {
    return bucket_remove_w(ht + bu, addr);
  }
}



INLINED uint32_t bucket_count_r(bucket_t * bu, uintptr_t addr) {
  uint32_t i, empty = bu->empty;
  for (i = 0; i < empty; i++) {
    if (bu->addr[i] == addr) {
      return bu->entry[i].num_readers;
    }
  }
  if (bu->next != NULL) {
    return bucket_count_r(bu->next, addr);
  }

  return 0;
}

INLINED uint32_t ssht_count_r(ssht_hashtable_t ht, uint32_t bu, uintptr_t addr) {
  if (ht == NULL) {
    printf("ht is null . . ., why?");
  }
  return bucket_count_r(ht + bu, addr);
}


INLINED void ht_print(bucket_t * ht) {
  unsigned int i;
  for (i = 0; i < NUM_BUCKETS; i++) {
    printf("[%02d]: ", i);
    bucket_print(ht + i);
  }
}

#endif /* _HT_H_ */
