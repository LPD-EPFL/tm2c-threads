#include "lock_log.h"

inline lock_log_set_t * lock_log_set_new() {
    lock_log_set_t *lock_log_set;

    if ((lock_log_set = (lock_log_set_t *) malloc(sizeof (lock_log_set_t))) == NULL) {
        PRINTD("Could not initialize the lock_log set");
        return NULL;
    }

    if ((lock_log_set->lock_log_entries = (lock_log_entry_t *) malloc(LOCK_SET_SIZE * sizeof (lock_log_entry_t))) == NULL) {
        free(lock_log_set);
        PRINTD("Could not initialize the lock_log set");
        return NULL;
    }

    lock_log_set->nb_entries = 0;
    lock_log_set->size = LOCK_SET_SIZE;

    return lock_log_set;
}

inline void lock_log_set_free(lock_log_set_t *lock_log_set) {
    free(lock_log_set->lock_log_entries);
    free(lock_log_set);
}

inline lock_log_set_t * lock_log_set_empty(lock_log_set_t *lock_log_set) {

    if (lock_log_set->size > LOCK_SET_SIZE) {
        lock_log_entry_t * temp;
        if ((temp = (lock_log_entry_t *) realloc(lock_log_set->lock_log_entries, LOCK_SET_SIZE * sizeof (lock_log_entry_t))) == NULL) {
            free(lock_log_set->lock_log_entries);
            PRINT("realloc @ lock_log_set_empty failed");
            lock_log_set->lock_log_entries = (lock_log_entry_t *) malloc(LOCK_SET_SIZE * sizeof (lock_log_entry_t));
            if (lock_log_set->lock_log_entries == NULL) {
                PRINT("malloc lock_log_set->lock_log_entries @ lock_log_set_empty");
                return NULL;
            }
        }
    }
    lock_log_set->size = LOCK_SET_SIZE;
    lock_log_set->nb_entries = 0;
    return lock_log_set;
}

inline lock_log_entry_t * lock_log_set_entry(lock_log_set_t *lock_log_set) {
    if (lock_log_set->nb_entries == lock_log_set->size) {
        //PRINTD("LOCK set max sized (%d)", lock_log_set->size);
        unsigned int new_size = 2 * lock_log_set->size;
        lock_log_entry_t *temp;
        if ((temp = (lock_log_entry_t *) realloc(lock_log_set->lock_log_entries, new_size * sizeof (lock_log_entry_t))) == NULL) {
            lock_log_set_free(lock_log_set);
            PRINTD("Could not resize the lock_log set");
            return NULL;
        }

        lock_log_set->lock_log_entries = temp;
        lock_log_set->size = new_size;
    }

    return &lock_log_set->lock_log_entries[lock_log_set->nb_entries++];
}

inline void lock_log_set_insert(lock_log_set_t *lock_log_set, entry_addr_t address, unsigned short index, unsigned short rw) {
    lock_log_entry_t *we = lock_log_set_entry(lock_log_set);
    we->address = address;
    we->index = index;
    we->rw = rw;
}

inline void lock_log_set_update(lock_log_set_t *lock_log_set, entry_addr_t address, unsigned short index, unsigned short rw) {
    unsigned int i;
    for (i = 0; i < lock_log_set->nb_entries; i++) {
        if ((lock_log_set->lock_log_entries[i].address == address) && (lock_log_set->lock_log_entries[i].index==index)) {
            lock_log_set->lock_log_entries[i].rw |= rw;
            return;
        }
    }

    lock_log_set_insert(lock_log_set, address, index, rw);
}

inline lock_log_entry_t * lock_log_set_contains(lock_log_set_t *lock_log_set, entry_addr_t address, unsigned short index) {
    unsigned int i;
    for (i = lock_log_set->nb_entries; i-- > 0; ) {
        if ((lock_log_set->lock_log_entries[i].address == address) && (lock_log_set->lock_log_entries[i].index==index)) {
            return &lock_log_set->lock_log_entries[i];
        }
    }

    return NULL;
}


