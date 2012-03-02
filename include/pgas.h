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

#include "common.h"

#define SHMEM_SIZE      (2048)           //1 KB
//#define SHMEM_SIZE      ((int) (3840 /NUM_DSL_NODES))           //1 KB
#define SHMEM           shmem__
#define PGAS_TYPE       long long
#define PGAS_GRAN       sizeof(long long int)
    typedef uintptr_t pgas_addr_t;
    //#define NUM_DSL_NODES 2

//    extern void * SHMEM;
    extern int * SHMEM;
    extern unsigned int shmem_index;
    extern unsigned int id__m1d2;
    extern unsigned int num_ues_d2;

    extern void PGAS_init();
    extern void PGAS_finalize();
    extern unsigned int PGAS_size();
    extern void PGAS_free(void *);

#define SHRINK(addr)    ((void*)(((uintptr_t)(addr)+(ptrdiff_t)(1))/NUM_DSL_NODES))

#define PGAS_write(addr, val, type)             \
        *(SHMEM + addr) = (val)
        //*((type *) ((type *) SHMEM + addr)) = (type) (val)
    //*((type *) ((type *) SHMEM + ROUND((double) (addr) / NUM_DSL_NODES))) = (type) (val)

#define PGAS_read(addr)                         \
        (SHMEM + addr)
//        *((int *) SHMEM + addr)
    //        *((int *) SHMEM + ROUND((double) (addr) / NUM_DSL_NODES))



    /*  for application cores ---------------------------------------------------------------
     */


#define PGAS_alloc_init(offset)                 \
            shmem_index = offset;               \
            id__m1d2 = (((RCCE_ue()) - 1)>>1);  \
            num_ues_d2 = (RCCE_num_ues())>>1;

#define PGAS_alloc_offs(offs)                   \
        id__m1d2 = (((RCCE_ue())-1)>>1) + offs; \
    
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

