/* 
 * File:   pgas.h
 * Author: trigonak
 *
 * header file for the PGAS memory model implementation on TM2C
 * 
 * Created on November 3, 2011, 10:47 AM
 */

#ifndef PGAS_H
#define	PGAS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <inttypes.h>

#include "common.h"
#include "fakemem.h"

extern const size_t SHMEM_SIZE;       /* size of the shared memory held by node */
extern const size_t PGAS_GRANULARITY; /* the length of consecutive data stored
                                         on a node */
extern int * SHMEM;
extern unsigned int shmem_index;
extern unsigned int id__m1d2; //the dslock core closest to the current core
extern unsigned int num_ues_d2; //number of cores divided by 2

extern void PGAS_init();
extern void PGAS_finalize();
extern size_t PGAS_size();
extern void PGAS_free(void *);


typedef uintptr_t pgas_addr_t;

/*
 * XXX: This needs to take into the account the granularity
 */
static inline tm_intern_addr_t
SHRINK(tm_intern_addr_t addr)
{
  //return (tm_intern_addr_t)((uintptr_t)addr/NUM_DSL_NODES);
  return addr;
}

/*
 * PGAS_write write to the pgas store, for the address represented by internal
 * address addr. It does its own translation.
 */
static inline void
PGAS_write(tm_intern_addr_t addr, uint32_t val)
{
	tm_intern_addr_t off = SHRINK(addr);
	//	PRINT("%d -> %d, writing %d", addr, off, val);
	if (off > SHMEM_SIZE) {
		PRINT("PGAS_write: write outside of boundaries for PGAS memory: "
			  " max %"PRIxIA", addr %"PRIxIA"\n",
			  SHMEM_SIZE,
			  off);
		EXIT(139);
	}
	uint32_t* ptr = (void*)((uintptr_t)SHMEM + off);
	*ptr = val;
}

static inline uint32_t*
PGAS_read(tm_intern_addr_t addr)
{
	tm_intern_addr_t off = SHRINK(addr);
	if (off > SHMEM_SIZE) {
		PRINT("PGAS_read: read outside of boundaries for PGAS memory: "
			  " max %"PRIxIA", addr %"PRIxIA"\n",
			  SHMEM_SIZE,
			  off);
		EXIT(139);
	}
	void* ptr = (void*)((uintptr_t)SHMEM + off);
	return (uint32_t*)ptr;
}

    /*  for application cores ---------------------------------------------------------------
     *  XXX: this allocation scheme works only for 50% app / 50% dslock core allocation
     */


#define PGAS_alloc_init(offset)                 \
            shmem_index = offset;               \
            id__m1d2 = (((NODE_ID()) - 1)>>1);  \
            num_ues_d2 = (TOTAL_NODES())>>1;

#define PGAS_alloc_offs(offs)                   \
        id__m1d2 = (((NODE_ID())-1)>>1) + offs; \
    
#define PGAS_alloc_id(id)                       \
        id__m1d2 = (((id) - 1)>>1)

#define PGAS_alloc_num_ues(num_ues)             \
        num_ues_d2 = (num_ues)>>1

#define PGAS_alloc_seq()                        \
        shmem_index++
    
#define PGAS_alloc()                            \
        ((id__m1d2) + (num_ues_d2 * shmem_index++))
    //((id__m1d2) + (shmem_index+=num_ues_d2))
    //((id__m1d2) + (num_ues_d2 * shmem_index++))
    //(((ID - 1)>>1) + ((NUM_UES>>1) * shmem_index++))

#ifdef	__cplusplus
}
#endif

#endif	/* PGAS_H */

