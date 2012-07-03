#ifndef ARRAY_LOG_H
#define	ARRAY_LOG_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "common.h"

#define ARRAY_SET_SIZE 1024

    typedef struct array_log_entry {
        uintptr_t address;
    } array_log_entry_t;

    typedef struct array_log_set {
        array_log_entry_t *array_log_entries;
        unsigned int nb_entries;
        unsigned int size;
    } array_log_set_t;

    extern array_log_set_t * array_log_set_new();

    extern void array_log_set_free(array_log_set_t *array_log_set);

    extern array_log_set_t * array_log_set_empty(array_log_set_t *array_log_set);

    inline array_log_entry_t * array_log_set_entry(array_log_set_t *array_log_set);

    extern void array_log_set_insert(array_log_set_t *array_log_set, uintptr_t address);

    extern array_log_entry_t * array_log_set_contains(array_log_set_t *array_log_set, uintptr_t address);


#ifdef	__cplusplus
}
#endif

#endif	/* LOG_H */

