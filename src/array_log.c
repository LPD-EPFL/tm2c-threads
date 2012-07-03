#include "array_log.h"

inline array_log_set_t * array_log_set_new() {
    array_log_set_t *array_log_set;

    if ((array_log_set = (array_log_set_t *) malloc(sizeof (array_log_set_t))) == NULL) {
        PRINTD("Could not initialize the array_log set");
        return NULL;
    }

    if ((array_log_set->array_log_entries = (array_log_entry_t *) malloc(ARRAY_SET_SIZE * sizeof (array_log_entry_t))) == NULL) {
        free(array_log_set);
        PRINTD("Could not initialize the array_log set");
        return NULL;
    }

    array_log_set->nb_entries = 0;
    array_log_set->size = ARRAY_SET_SIZE;

    return array_log_set;
}

inline void array_log_set_free(array_log_set_t *array_log_set) {
    free(array_log_set->array_log_entries);
    free(array_log_set);
}

inline array_log_set_t * array_log_set_empty(array_log_set_t *array_log_set) {

    if (array_log_set->size > ARRAY_SET_SIZE) {
        array_log_entry_t * temp;
        if ((temp = (array_log_entry_t *) realloc(array_log_set->array_log_entries, ARRAY_SET_SIZE * sizeof (array_log_entry_t))) == NULL) {
            free(array_log_set->array_log_entries);
            PRINT("realloc @ array_log_set_empty failed");
            array_log_set->array_log_entries = (array_log_entry_t *) malloc(ARRAY_SET_SIZE * sizeof (array_log_entry_t));
            if (array_log_set->array_log_entries == NULL) {
                PRINT("malloc array_log_set->array_log_entries @ array_log_set_empty");
                return NULL;
            }
        }
    }
    array_log_set->size = ARRAY_SET_SIZE;
    array_log_set->nb_entries = 0;
    return array_log_set;
}

inline array_log_entry_t * array_log_set_entry(array_log_set_t *array_log_set) {
    if (array_log_set->nb_entries == array_log_set->size) {
        //PRINTD("ARRAY set max sized (%d)", array_log_set->size);
        unsigned int new_size = 2 * array_log_set->size;
        array_log_entry_t *temp;
        if ((temp = (array_log_entry_t *) realloc(array_log_set->array_log_entries, new_size * sizeof (array_log_entry_t))) == NULL) {
            array_log_set_free(array_log_set);
            PRINTD("Could not resize the array_log set");
            return NULL;
        }

        array_log_set->array_log_entries = temp;
        array_log_set->size = new_size;
    }

    return &array_log_set->array_log_entries[array_log_set->nb_entries++];
}

inline void array_log_set_insert(array_log_set_t *array_log_set, uintptr_t address) {
    array_log_entry_t *we = array_log_set_entry(array_log_set);
    we->address = address;
}

inline array_log_entry_t * array_log_set_contains(array_log_set_t *array_log_set, uintptr_t address) {
    unsigned int i;
    for (i = array_log_set->nb_entries; i-- > 0; ) {
        if (array_log_set->array_log_entries[i].address == address) {
            return &array_log_set->array_log_entries[i];
        }
    }

    return NULL;
}


