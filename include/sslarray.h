#ifndef _HT_H_
#define _HT_H_


#define MOVE_EMPTY

#include <inttypes.h>
#include "sslarray_log.h"

#ifndef INLINED
#if __GNUC__ && !__GNUC_STDC_INLINE__
#define INLINED static inline __attribute__((always_inline))
#else
#define INLINED inline
#endif
#endif

#define NO_WRITER 0x1000
#define ENTRY_FREE 0x10000000 //decimal: 268435456

#define FALSE 0
#define TRUE 1


#define SSLARRAY_SIZE 10*1024*1024

extern unsigned int num_moves;
extern unsigned int cur_size, max_size;

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

typedef entry_t * sslarray_t;

INLINED sslarray_t sslarray_new() {
  sslarray_t hashtable;
  hashtable = (sslarray_t) malloc(SSLARRAY_SIZE * sizeof(entry_t));
  assert(hashtable != NULL);

  uint32_t i;
  for (i = 0; i < SSLARRAY_SIZE; i++) {
    hashtable[i].whole = ENTRY_FREE;
  }

  return hashtable;
}

INLINED CONFLICT_TYPE bucket_insert_r(sslarray_t ar, uint32_t addr) {
  if (ar[addr].writer == NO_WRITER) {
    ar[addr].num_readers++;
    return NO_CONFLICT;
  }
  else {
    return READ_AFTER_WRITE;
  }

  return NO_CONFLICT;
}

INLINED CONFLICT_TYPE bucket_insert_w(sslarray_t ar, uint32_t addr, uint32_t id) {
  if (ar[addr].num_readers > 0) {
    return WRITE_AFTER_READ;
  }
  else if (ar[addr].writer != NO_WRITER) {
    return WRITE_AFTER_WRITE;
  }

  ar[addr].writer = id;
  return NO_CONFLICT;
}


INLINED CONFLICT_TYPE sslarray_insert(sslarray_t ar, sslarray_log_set_t *log, uint32_t id, uint32_t addr, RW rw) {
  CONFLICT_TYPE ct;
  if (rw == READ) {
    ct = bucket_insert_r(ar, addr);
  }
  else {
    ct = bucket_insert_w(ar, addr, id);
  }

  if (ct == NO_CONFLICT) {
    sslarray_log_set_insert(log, addr);
  }

  return ct;
}


INLINED uint32_t sslarray_remove(sslarray_t ar, uint32_t addr) {
  if (ar[addr].writer != NO_WRITER) {
    ar[addr].writer = NO_WRITER;
    return TRUE;
  }
  else if (ar[addr].num_readers > 0) {
    ar[addr].num_readers--;
    return TRUE;
  }

  return FALSE;
}

INLINED void sslarray_print(sslarray_t ar) {
  uint32_t i;
  for (i = 0; i < SSLARRAY_SIZE; i++) {
    printf("%2d: %d/%d | ", i, ar[i].num_readers, ar[i].writer);
  }
  printf("\n");
}

#endif /* _HT_H_ */
