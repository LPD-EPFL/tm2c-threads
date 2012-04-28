/* 
 * File:   pgas.c
 * Author: trigonak
 *
 * Created on November 3, 2011, 10:53 AM
 */

#include "pgas.h"

int * SHMEM;

const size_t SHMEM_SIZE = 64 * 1024 * 1024 * sizeof(int); /* 64 M ints */

unsigned int shmem_index = 0;
unsigned int id__m1d2;
unsigned int num_ues_d2;

void PGAS_init() {
  //    fprintf(stderr, "SHMEM_SIZE: %zu\n", SHMEM_SIZE);
    SHMEM = (int *) calloc(SHMEM_SIZE/sizeof(int), sizeof(int));
    //SHMEM = (int *) malloc(SHMEM_SIZE);

    if (SHMEM == NULL) {
        PRINT("malloc @ PGAS_init");
        EXIT(-1);
    }
    
    // bzero(SHMEM, SHMEM_SIZE);
    
    PRINT("allocated %zu bytes for PGAS shmem, position %p", SHMEM_SIZE, SHMEM);
}

void PGAS_finalize() {
    free(SHMEM);
}

size_t PGAS_size() {
    return SHMEM_SIZE;
}

