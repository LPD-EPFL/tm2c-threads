#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "mcore_malloc.h"

#define MAX_FILENAME_LENGTH 100

static MCORE_BLOCK_S MCORE_space;   // data structure used for tracking MPB memory blocks
static MCORE_BLOCK_S *MCORE_spacep; // pointer to MCORE_space

//--------------------------------------------------------------------------------------
// FUNCTION: MCORE_shmalloc_init
//--------------------------------------------------------------------------------------
// initialize memory allocator
//--------------------------------------------------------------------------------------
// size (bytes) of managed space
void
MCORE_shmalloc_init(size_t size)
{
   //create the shared space which will be managed by the allocator

   char keyF[MAX_FILENAME_LENGTH];
   sprintf(keyF,"/mcore_mem2");

   int shmfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
   if (shmfd<0)
   {
      if (errno != EEXIST)
      {
         perror("In shm_open");
         exit(1);
      }

      //this time it is ok if it already exists
      shmfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (shmfd<0)
      {
         perror("In shm_open");
         exit(1);
      }
   }
   else
   {
      //only if it is just created
     if (!ftruncate(shmfd,size))
       {
	 printf("ftruncate failed\n");
       }
   }

  t_vcharp mem = (t_vcharp) mmap(NULL, size,PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

  // create one block containing all memory for truly dynamic memory allocator
  MCORE_spacep = &MCORE_space;
  MCORE_spacep->tail = (MCORE_BLOCK *) malloc(sizeof(MCORE_BLOCK));
  MCORE_spacep->tail->free_size = size;
  MCORE_spacep->tail->space = mem;
  /* make a circular list by connecting tail to itself */
  MCORE_spacep->tail->next = MCORE_spacep->tail;
#ifdef SHMDBG 
printf("%s: %d: MCORE_spacep->tail->free_size, MCORE_spacep->tail->space: %x %x\n",
__FILE__, __LINE__,MCORE_spacep->tail->free_size, MCORE_spacep->tail->space);
#endif
}

//--------------------------------------------------------------------------------------
// FUNCTION: MCORE_shmalloc
//--------------------------------------------------------------------------------------
// Allocate memory in off-chip shared memory. This is a collective call that should be
// issued by all participating cores if consistent results are required. All cores will
// allocate space that is exactly overlapping. Alternatively, determine the beginning of
// the off-chip shared memory on all cores and subsequently let just one core do all the
// allocating and freeing. It can then pass offsets to other cores who need to know what
// shared memory regions were involved.
//--------------------------------------------------------------------------------------
 // requested space
t_vcharp
MCORE_shmalloc(size_t size)
{

  t_vcharp result;

  // simple memory allocator, loosely based on public domain code developed by
  // Michael B. Allen and published on "The Scripts--IT /Developers Network".
  // Approach: 
  // - maintain linked list of pointers to memory. A block is either completely
  //   malloced (free_size = 0), or completely free (free_size > 0).
  //   The space field always points to the beginning of the block
  // - malloc: traverse linked list for first block that has enough space    
  // - free: Check if pointer exists. If yes, check if the new block should be 
  //         merged with neighbors. Could be one or two neighbors.

  MCORE_BLOCK *b1, *b2, *b3;   // running pointers for blocks              

  // Unlike the MPB, the off-chip shared memory is uncached by default, so can
  // be allocated in any increment, not just the cache line size
  if (size==0) return 0;

  // always first check if the tail block has enough space, because that
  // is the most likely. If it does and it is exactly enough, we still
  // create a new block that will be the new tail, whose free space is 
  // zero. This acts as a marker of where free space of predecessor ends   
//printf("MCORE_spacep->tail: %x\n",MCORE_spacep->tail);
  b1 = MCORE_spacep->tail;
  if (b1->free_size >= size) {
    // need to insert new block; new order is: b1->b2 (= new tail)         
    b2 = (MCORE_BLOCK *) malloc(sizeof(MCORE_BLOCK));
    b2->next      = b1->next;
    b1->next      = b2;
    b2->free_size = b1->free_size-size;
    b2->space     = b1->space + size;
    b1->free_size = 0;
    // need to update the tail                                             
    MCORE_spacep->tail = b2;
    return(b1->space);
  }

  // tail didn't have enough space; loop over whole list from beginning    
  while (b1->next->free_size < size) {
    if (b1->next == MCORE_spacep->tail) {
      return NULL; // we came full circle 
    }
    b1 = b1->next;
  }

  b2 = b1->next;
  if (b2->free_size > size) { // split block; new block order: b1->b2->b3  
    b3            = (MCORE_BLOCK *) malloc(sizeof(MCORE_BLOCK));
    b3->next      = b2->next; // reconnect pointers to add block b3        
    b2->next      = b3;       //     "         "     "  "    "    "        
    b3->free_size = b2->free_size - size; // b3 gets remainder free space  
    b3->space     = b2->space + size; // need to shift space pointer       
  } 
  b2->free_size = 0;          // block b2 is completely used               
  return (b2->space);
}

//--------------------------------------------------------------------------------------
// FUNCTION: MCORE_shfree
//--------------------------------------------------------------------------------------
// Deallocate memory in off-chip shared memory. Also collective, see MCORE_shmalloc
//--------------------------------------------------------------------------------------
// pointer to data to be freed
void
MCORE_shfree(t_vcharp ptr)
{
  MCORE_BLOCK *b1, *b2, *b3;   // running block pointers                    
  int j1, j2;                 // booleans determining merging of blocks    

  // loop over whole list from the beginning until we locate space ptr     
  b1 = MCORE_spacep->tail;
  while (b1->next->space != ptr && b1->next != MCORE_spacep->tail) { 
    b1 = b1->next;
  }

  // b2 is target block whose space must be freed    
  b2 = b1->next;              
  // tail either has zero free space, or hasn't been malloc'ed             
  if (b2 == MCORE_spacep->tail) return;      

  // reset free space for target block (entire block)                      
  b3 = b2->next;
  b2->free_size = b3->space - b2->space;

  // determine with what non-empty blocks the target block can be merged   
  j1 = (b1->free_size>0 && b1!=MCORE_spacep->tail); // predecessor block    
  j2 = (b3->free_size>0 || b3==MCORE_spacep->tail); // successor block      

  if (j1) {
    if (j2) { // splice all three blocks together: (b1,b2,b3) into b1      
      b1->next = b3->next;
      b1->free_size +=  b3->free_size + b2->free_size;
      if (b3==MCORE_spacep->tail) MCORE_spacep->tail = b1;
      free(b3);
    } 
    else {    // only merge (b1,b2) into b1                                
      b1->free_size += b2->free_size;
      b1->next = b3;
    }
    free(b2);
  } 
  else {
    if (j2) { // only merge (b2,b3) into b2                                
      b2->next = b3->next;
      b2->free_size += b3->free_size;
      if (b3==MCORE_spacep->tail) MCORE_spacep->tail = b2;
      free(b3);
    } 
  }
}
