/* 
 * File:   mem.h
 * Author: trigonak
 *
 * Created on May 23, 2011, 10:55 AM
 */

#ifndef MEM_H
#define	MEM_H

#ifdef	__cplusplus
extern "C" {
#endif

    /*
     * File:
     *   mod_mem.c
     * Author(s):
     *   Pascal Felber <pascal.felber@unine.ch>
     *   Vincent Gramoli <vincent.gramoli@epfl.ch>
     * Description:
     *   Module for dynamic memory management.
     *
     * Copyright (c) 2007-2009.
     *
     * This program is free software; you can redistribute it and/or
     * modify it under the terms of the GNU General Public License
     * as published by the Free Software Foundation, version 2
     * of the License.
     *
     * This program is distributed in the hope that it will be useful,
     * but WITHOUT ANY WARRANTY; without even the implied warranty of
     * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     * GNU General Public License for more details.
     */

#include "common.h"

    /* ################################################################### *
     * TYPES
     * ################################################################### */

    typedef unsigned int * stm_word_t;

    typedef struct mem_block { /* Block of allocated memory */
        void *addr; /* Address of memory */
        struct mem_block *next; /* Next block */
    } mem_block_t;

    typedef struct mem_info { /* Memory descriptor */
        mem_block_t *allocated; /* Memory allocated by this transaction (freed upon abort) */
        mem_block_t *allocated_shmem; /* Shared Memory allocated by this transaction (freed upon abort) */
        mem_block_t *freed; /* Memory freed by this transaction (freed upon commit) */
        mem_block_t *freed_shmem; /* Shared Memory freed by this transaction (freed upon commit) */
    } mem_info_t;

    /* ################################################################### *
     * FUNCTIONS
     * ################################################################### */

    /*
     * Called by the CURRENT thread to allocate memory within a transaction.
     */
    INLINED void *stm_malloc(mem_info_t *stm_mem_info, size_t size) {
        /* Memory will be freed upon abort */
        mem_block_t *mb; 

        /* Round up size */
        //size = (size + 3) & ~(size_t) 0x03;

        if ((mb = (mem_block_t *) malloc(sizeof (mem_block_t))) == NULL) {
            PRINT("malloc @ stm_malloc");
            EXIT(1);
        }
        if ((mb->addr = malloc(size)) == NULL) {
            free(mb);
            PRINT("malloc @ stm_malloc");
            EXIT(1);
        }
        mb->next = stm_mem_info->allocated;
        stm_mem_info->allocated = mb;

        return mb->addr;
    }

    /*
     * Called by the CURRENT thread to allocate shared memory within a transaction.
     */
    INLINED void *stm_shmalloc(mem_info_t *stm_mem_info, size_t size) {
        /* Memory will be freed upon abort */
        mem_block_t *mb; 

        /* Round up size */
        //size = (size + 3) & ~(size_t) 0x03;

        if ((mb = (mem_block_t *) malloc(sizeof (mem_block_t))) == NULL) {
            PRINT("malloc @ stm_shmalloc");
            EXIT(1);
        }
        if ((mb->addr = (void *) RCCE_shmalloc(size)) == NULL) {
            free(mb);
            PRINT("RCCE_shmalloc @ stm_shmalloc");
            EXIT(1);
        }
        mb->next = stm_mem_info->allocated_shmem;
        stm_mem_info->allocated_shmem = mb;

        return mb->addr;
    }

    /*
     * Called by the CURRENT thread to free memory within a transaction.
     */
    INLINED void stm_free(mem_info_t *stm_mem_info, void *addr) {
        /* Memory disposal is delayed until commit */
        mem_block_t *mb; 

        /* Schedule for removal */
        if ((mb = (mem_block_t *) malloc(sizeof (mem_block_t))) == NULL) {
            PRINT("malloc @ stm_free");
            EXIT(1);
        }
        mb->addr = addr;
        mb->next = stm_mem_info->freed;
        stm_mem_info->freed = mb;
    }

    /*
     * Called by the CURRENT thread to free memory within a transaction.
     */
    INLINED void stm_shfree(mem_info_t *stm_mem_info, t_vcharp addr) {
        /* Memory disposal is delayed until commit */
        mem_block_t *mb; 

        /* Schedule for removal */
        if ((mb = (mem_block_t *) malloc(sizeof (mem_block_t))) == NULL) {
            PRINT("malloc @ stm_shfree");
            EXIT(1);
        }
        mb->addr = (void *) addr;
        mb->next = stm_mem_info->freed_shmem;
        stm_mem_info->freed_shmem = mb;
    }

    /*
     * Called to create new mem_info
     */
    INLINED mem_info_t * mem_info_new() {
        mem_info_t *mi__;

        if ((mi__ = (mem_info_t *) malloc(sizeof (mem_info_t))) == NULL) {
            PRINT("malloc @ mem_info_new");
            EXIT(1);
        }
        mi__->allocated = mi__->freed = NULL;
        mi__->allocated_shmem = mi__->freed_shmem = NULL;

        return mi__;
    }
    
    
    /*
     * Called to free the memory info
     */
    INLINED void mem_info_free(mem_info_t * mi) {
        free(mi);
    }
    
    /*
     * Called to free the global stm_mem_info variable
     */
    INLINED void stm_mem_info_free(mem_info_t *stm_mem_info) {
        free(stm_mem_info);
    }


    /*
     * Called upon transaction commit.
     */
    INLINED void mem_info_on_commit(mem_info_t *stm_mem_info) {
        mem_block_t *mb, *next;

        /* Keep memory allocated during transaction */
        if (stm_mem_info->allocated != NULL) {
            mb = stm_mem_info->allocated;
            while (mb != NULL) {
                next = mb->next;
                free(mb);
                mb = next;
            }
            stm_mem_info->allocated = NULL;
        }

        /* Keep shared memory allocated during transaction */
        if (stm_mem_info->allocated_shmem != NULL) {
            mb = stm_mem_info->allocated_shmem;
            while (mb != NULL) {
                next = mb->next;
                free(mb);
                mb = next;
            }
            stm_mem_info->allocated_shmem = NULL;
        }

        /* Dispose of memory freed during transaction */
        if (stm_mem_info->freed != NULL) {
            mb = stm_mem_info->freed;
            while (mb != NULL) {
                next = mb->next;
                free(mb->addr);
                free(mb);
                mb = next;
            }
            stm_mem_info->freed = NULL;
        }

        /* Dispose of shared memory freed during transaction */
        if (stm_mem_info->freed_shmem != NULL) {
            mb = stm_mem_info->freed_shmem;
            while (mb != NULL) {
                next = mb->next;
                RCCE_shfree((t_vcharp) mb->addr);
                free(mb);
                mb = next;
            }
            stm_mem_info->freed_shmem = NULL;
        }
    }

    /*
     * Called upon transaction abort.
     */
    INLINED void mem_info_on_abort(mem_info_t *stm_mem_info) {
        mem_block_t *mb, *next;

        /* Dispose of memory allocated during transaction */
        if (stm_mem_info->allocated != NULL) {
            mb = stm_mem_info->allocated;
            while (mb != NULL) {
                next = mb->next;
                free(mb->addr);
                free(mb);
                mb = next;
            }
            stm_mem_info->allocated = NULL;
        }

        /* Dispose of shared memory allocated during transaction */
        if (stm_mem_info->allocated_shmem != NULL) {
            mb = stm_mem_info->allocated_shmem;
            while (mb != NULL) {
                next = mb->next;
                RCCE_shfree((t_vcharp) mb->addr);
                free(mb);
                mb = next;
            }
            stm_mem_info->allocated_shmem = NULL;
        }

        /* Keep memory freed during transaction */
        if (stm_mem_info->freed != NULL) {
            mb = stm_mem_info->freed;
            while (mb != NULL) {
                next = mb->next;
                free(mb);
                mb = next;
            }
            stm_mem_info->freed = NULL;
        }

        /* Keep shared memory freed during transaction */
        if (stm_mem_info->freed_shmem != NULL) {
            mb = stm_mem_info->freed_shmem;
            while (mb != NULL) {
                next = mb->next;
                free(mb);
                mb = next;
            }
            stm_mem_info->freed_shmem = NULL;
        }
    }

#ifdef	__cplusplus
}
#endif

#endif	/* MEM_H */

