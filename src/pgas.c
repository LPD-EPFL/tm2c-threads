/* 
 * File:   pgas.c
 * Author: trigonak
 *
 * Created on November 3, 2011, 10:53 AM
 */

#include "pgas.h"

void * SHMEM;

void PGAS_init() {
    SHMEM = (void *) malloc(SHMEM_SIZE);
    if (SHMEM == NULL) {
        PRINT("malloc @ PGAS_init");
        EXIT(-1);
    }
    
    int i;
    for (i = 0; i < SHMEM_SIZE; i++) {
        *((int *) SHMEM + i) = i;
    }
    for (i = 0; i < SHMEM_SIZE; i++) {
        *((int *) SHMEM + i) += 1;
    }
    
    PRINT("allocated %u bytes for shmem", SHMEM_SIZE);
}

void PGAS_finalize() {
    free(SHMEM);
}

unsigned int PGAS_size() {
    return SHMEM_SIZE;
}

