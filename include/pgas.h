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

#define SHMEM_SIZE      (1024)           //1 KB
#define SHMEM           shmem__
    //#define NUM_DSL_NODES   2
#define PGAS_TYPE       long long
#define PGAS_GRAN       sizeof(long long int)
//#define NUM_DSL_NODES 2

    extern void * SHMEM;

    extern void PGAS_init();
    extern void PGAS_finalize();
    extern unsigned int PGAS_size();
    extern void * PGAS_alloc();
    extern void PGAS_free(void *);

#define ROUND(n)        (((int) n) == n ? (int) n : (int) n + 1)

#define PGAS_write(addr, val, type)             \
        *((type *) ((type *) SHMEM + addr)) = (type) (val)
        //*((type *) ((type *) SHMEM + ROUND((double) (addr) / NUM_DSL_NODES))) = (type) (val)

#define PGAS_read(addr)                         \
        *((int *) SHMEM + addr)
//        *((int *) SHMEM + ROUND((double) (addr) / NUM_DSL_NODES))

//    inline int PGAS_read(unsigned int addr);




#ifdef	__cplusplus
}
#endif

#endif	/* PGAS_H */

