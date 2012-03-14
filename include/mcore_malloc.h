#ifndef MCORE_LIB_H
#define MCORE_LIB_H

#include <string.h>

typedef volatile unsigned char* t_vcharp;

typedef struct mcore_block {
  t_vcharp space;          // pointer to space for data in block             
  size_t free_size;        // actual free space in block (0 or whole block)  
  struct mcore_block *next; // pointer to next block in circular linked list 
} MCORE_BLOCK;

typedef struct  {
  MCORE_BLOCK *tail;     // "last" block in linked list of blocks           
} MCORE_BLOCK_S;

void MCORE_shmalloc_init(size_t size);
t_vcharp MCORE_shmalloc(size_t size);
void MCORE_shfree(t_vcharp ptr);

#endif
