/* 
 * File:   pgas.c
 * Author: trigonak
 *
 * Created on November 3, 2011, 10:53 AM
 */

#include "pgas.h"

int * SHMEM;
/*
void * SHMEM;
*/
unsigned int shmem_index = 0;
unsigned int id__m1d2;
unsigned int num_ues_d2;

void PGAS_init() {
    SHMEM = (int *) malloc(SHMEM_SIZE);
/*
    SHMEM = (void *) malloc(SHMEM_SIZE);
*/
    if (SHMEM == NULL) {
        PRINT("malloc @ PGAS_init");
        EXIT(-1);
    }
    
    bzero(SHMEM, SHMEM_SIZE);
    
    PRINT("allocated %u bytes for PGAS shmem", SHMEM_SIZE);
}

void PGAS_finalize() {
    free(SHMEM);
}

unsigned int PGAS_size() {
    return SHMEM_SIZE;
}

