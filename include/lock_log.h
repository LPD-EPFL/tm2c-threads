#ifndef LOCK_LOG_H
#define	LOCK_LOG_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "common.h"

#define LOCK_SET_SIZE 1024

    typedef uintptr_t entry_addr_t;
    typedef struct lock_log_entry {
        //the address of the hash entry holding the rw_entry
        entry_addr_t address;
        //the id in the entry of the data of interest
        unsigned short index;
        //read and/or lock_log
        unsigned short rw;
    } lock_log_entry_t;

    typedef struct lock_log_set {
        lock_log_entry_t *lock_log_entries;
        unsigned int nb_entries;
        unsigned int size;
    } lock_log_set_t;

    extern lock_log_set_t * lock_log_set_new();

    extern void lock_log_set_free(lock_log_set_t *lock_log_set);

    extern lock_log_set_t * lock_log_set_empty(lock_log_set_t *lock_log_set);

    inline lock_log_entry_t * lock_log_set_entry(lock_log_set_t *lock_log_set);

    extern void lock_log_set_insert(lock_log_set_t *lock_log_set, entry_addr_t address, unsigned short index, unsigned short rw);

    extern void lock_log_set_update(lock_log_set_t *lock_log_set, entry_addr_t address, unsigned short index, unsigned short rw);

    extern lock_log_entry_t * lock_log_set_contains(lock_log_set_t *lock_log_set, entry_addr_t address, unsigned short index);


#ifdef	__cplusplus
}
#endif

#endif	/* LOG_H */

