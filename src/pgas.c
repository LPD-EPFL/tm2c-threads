/* 
 * File:   pgas.c
 * Author: trigonak
 *
 * Created on November 3, 2011, 10:53 AM
 */

#include "pgas.h"

int * SHMEM;

const size_t SHMEM_SIZE = 16 * 1024 * 1024 * sizeof(int); /* 16 M ints */

unsigned int shmem_index = 0;
unsigned int id__m1d2;
unsigned int num_ues_d2;

void PGAS_init() {
    fprintf(stderr, "SHMEM_SIZE: %d\n", SHMEM_SIZE);
    SHMEM = (int *) malloc(SHMEM_SIZE);
/*
    SHMEM = (void *) malloc(SHMEM_SIZE);
*/
    if (SHMEM == NULL) {
        PRINT("malloc @ PGAS_init");
        EXIT(-1);
    }
    
    bzero(SHMEM, SHMEM_SIZE);
    
    PRINT("allocated %u bytes for PGAS shmem, position %p", SHMEM_SIZE, SHMEM);
}

void PGAS_finalize() {
    free(SHMEM);
}

size_t PGAS_size() {
    return SHMEM_SIZE;
}

