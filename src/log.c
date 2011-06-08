/* 
 * File:   log.c
 * Author: trigonak
 *
 * Created on June 01, 2011, 6:33PM
 */

#include "log.h"

inline write_set_t * write_set_new() {
    write_set_t *write_set;

    if ((write_set = (write_set_t *) malloc(sizeof (write_set_t))) == NULL) {
        PRINTD("Could not initialize the write set");
        return NULL;
    }

    if ((write_set->write_entries = (write_entry_t *) malloc(WRITE_SET_SIZE * sizeof (write_entry_t))) == NULL) {
        free(write_set);
        PRINTD("Could not initialize the write set");
        return NULL;
    }
    
    write_set->nb_entries = 0;
    write_set->size = WRITE_SET_SIZE;

    return write_set;
}

inline void write_set_free(write_set_t *write_set) {
    free(write_set->write_entries);
    free(write_set);
}

inline write_entry_t * write_set_entry(write_set_t *write_set) {
    if (write_set->nb_entries == write_set->size) {
        //PRINTD("WRITE set max sized (%d)", write_set->size);
        unsigned int new_size = 2 * write_set->size;
        write_entry_t *temp;
        if ((temp = (write_entry_t *) realloc(write_set->write_entries, new_size * sizeof (write_entry_t))) == NULL) {
            write_set_free(write_set);
            PRINTD("Could not resize the write set");
            return NULL;
        }

        write_set->write_entries = temp;
        write_set->size = new_size;
    }

    return &write_set->write_entries[write_set->nb_entries++];
}

inline void write_entry_set_value(write_entry_t *we, void *value) {
    switch (we->datatype) {
        case TYPE_CHAR:
            we->c = CAST_CHAR(value);
            break;
        case TYPE_DOUBLE:
            we->d = CAST_DOUBLE(value);
            break;
        case TYPE_FLOAT:
            we->f = CAST_FLOAT(value);
            break;
        case TYPE_INT:
            we->i = CAST_INT(value);
            break;
        case TYPE_LONG:
            we->li = CAST_LONG(value);
            break;
        case TYPE_LONGLONG:
            we->lli = CAST_LONGLONG(value);
            break;
        case TYPE_SHORT:
            we->s = CAST_SHORT(value);
            break;
        case TYPE_UCHAR:
            we->uc = CAST_UCHAR(value);
            break;
        case TYPE_UINT:
            we->ui = CAST_UINT(value);
            break;
        case TYPE_ULONG:
            we->uli = CAST_ULONG(value);
            break;
        case TYPE_ULONGLONG:
            we->ulli = CAST_ULONGLONG(value);
            break;
        case TYPE_USHORT:
            we-> us = CAST_USHORT(value);
            break;
        case TYPE_POINTER:
            we->p = value;
            break;
        default: //pointer to a mem area that the we->datatype defines the length
            we->p = value;
    }
}

inline void write_set_insert(write_set_t *write_set, DATATYPE datatype, void *value, void *address_shmem) {
    write_entry_t *we = write_set_entry(write_set);

    we->datatype = datatype;
    we->address_shmem = address_shmem;
    write_entry_set_value(we, value);
}

inline void write_set_update(write_set_t *write_set, DATATYPE datatype, void *value, void *address_shmem) {
    int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        if (write_set->write_entries[i].address_shmem == address_shmem) {
            write_entry_set_value(&write_set->write_entries[i], value);
            return;
        }
    }

    write_set_insert(write_set, datatype, value, address_shmem);
}

inline void write_entry_persist(write_entry_t *we) {
    switch (we->datatype) {
        case TYPE_CHAR:
            CAST_CHAR(we->address_shmem) = we->c;
            break;
        case TYPE_DOUBLE:
            CAST_DOUBLE(we->address_shmem) = we->d;
            break;
        case TYPE_FLOAT:
            CAST_FLOAT(we->address_shmem) = we->f;
            break;
        case TYPE_INT:
            CAST_INT(we->address_shmem) = we->i;
            break;
        case TYPE_LONG:
            CAST_LONG(we->address_shmem) = we->li;
            break;
        case TYPE_LONGLONG:
            CAST_LONGLONG(we->address_shmem) = we->lli;
            break;
        case TYPE_SHORT:
            CAST_SHORT(we->address_shmem) = we->s;
            break;
        case TYPE_UCHAR:
            CAST_UCHAR(we->address_shmem) = we->uc;
            break;
        case TYPE_UINT:
            CAST_UINT(we->address_shmem) = we->ui;
            break;
        case TYPE_ULONG:
            CAST_ULONG(we->address_shmem) = we->uli;
            break;
        case TYPE_ULONGLONG:
            CAST_ULONGLONG(we->address_shmem) = we->ulli;
            break;
        case TYPE_USHORT:
            CAST_USHORT(we->address_shmem) = we->us;
            break;
        case TYPE_POINTER:
            we->address_shmem = we->p;
            break;
        default:
            memcpy(we->address_shmem, we->p, we->datatype);
    }
}

inline void write_entry_print(write_entry_t *we) {
    switch (we->datatype) {
        case TYPE_CHAR:
            PRINTSME("%p : %c", (we->address_shmem), we->c);
            break;
        case TYPE_DOUBLE:
            PRINTSME("%p : %f", (we->address_shmem), we->d);
            break;
        case TYPE_FLOAT:
            PRINTSME("%p : %f", (we->address_shmem), we->f);
            break;
        case TYPE_INT:
            PRINTSME("%p : %d", (we->address_shmem), we->i);
            break;
        case TYPE_LONG:
            PRINTSME("%p : %ld", (we->address_shmem), we->li);
            break;
        case TYPE_LONGLONG:
            PRINTSME("%p : %lld", (we->address_shmem), we->lli);
            break;
        case TYPE_SHORT:
            PRINTSME("%p : %i", (we->address_shmem), we->s);
            break;
        case TYPE_UCHAR:
            PRINTSME("%p : %c", (we->address_shmem), we->uc);
            break;
        case TYPE_UINT:
            PRINTSME("%p : %u", (we->address_shmem), we->ui);
            break;
        case TYPE_ULONG:
            PRINTSME("%p : %lu", (we->address_shmem), we->uli);
            break;
        case TYPE_ULONGLONG:
            PRINTSME("%p : %llu", (we->address_shmem), we->ulli);
            break;
        case TYPE_USHORT:
            PRINTSME("%p : %us", (we->address_shmem), we->us);
            break;
        case TYPE_POINTER:
            PRINTSME("%p : %p", we->address_shmem, we->p);
            break;
        default:
            PRINTSME("%p : %s", (char *) we->address_shmem, (const char *) we->p);
    }
}

inline void write_set_print(write_set_t *write_set) {
    PRINTSME("WRITE SET (elements: %d, size: %d) --------------\n", write_set->nb_entries, write_set->size);
    int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        write_entry_print(&write_set->write_entries[i]);
    }
    FLUSH
}

inline void write_set_persist(write_set_t *write_set) {
    int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        write_entry_persist(&write_set->write_entries[i]);
    }
}

inline write_entry_t * write_set_contains(write_set_t *write_set, void *address_shmem) {
    int i;
    for (i = write_set->nb_entries - 1; i >= 0; i--) {
        if (write_set->write_entries[i].address_shmem == address_shmem) {
            return &write_set->write_entries[i];
        }
    }

    return NULL;
}

/*______________________________________________________________________________________________________
 * READ SET                                                                                             |
 *______________________________________________________________________________________________________|
 */


inline read_set_t * read_set_new() {
    read_set_t *read_set;

    if ((read_set = (read_set_t *) malloc(sizeof (read_entry_l_t))) == NULL) {
        PRINTD("Could not initialize the read set");
        return NULL;
    }

    if ((read_set->read_entries = (read_entry_l_t *) malloc(READ_SET_SIZE * sizeof (read_entry_l_t))) == NULL) {
        free(read_set);
        PRINTD("Could not initialize the read set");
        return NULL;
    }

    read_set->nb_entries = 0;
    read_set->size = READ_SET_SIZE;

    return read_set;
}

inline void read_set_free(read_set_t *read_set) {
    free(read_set->read_entries);
    free(read_set);
}

inline read_entry_l_t * read_set_entry(read_set_t *read_set) {
    if (read_set->nb_entries == read_set->size) {
        unsigned int new_size = 2 * read_set->size;
        //PRINTD("READ set max sized (%d)", read_set->size);
        read_entry_l_t *temp;
        if ((temp = (read_entry_l_t *) realloc(read_set->read_entries, new_size * sizeof (read_entry_l_t))) == NULL) {
            PRINTD("Could not resize the write set");
            read_set_free(read_set);
            return NULL;
        }

        read_set->read_entries = temp;
        read_set->size = new_size;
    }

    return &read_set->read_entries[read_set->nb_entries++];
}

#ifdef READDATATYPE

inline void read_set_insert(read_set_t *read_set, DATATYPE datatype, void *address_shmem) {
#else

inline void read_set_insert(read_set_t *read_set, void *address_shmem) {
#endif
    read_entry_l_t *re = read_set_entry(read_set);
#ifdef READDATATYPE
    re->datatype = datatype;
#endif
    re->address_shmem = address_shmem;
}

#ifdef READDATATYPE

inline BOOLEAN read_set_update(read_set_t *read_set, DATATYPE datatype, void *address_shmem) {
#else

inline BOOLEAN read_set_update(read_set_t *read_set, void *address_shmem) {
#endif
    int i;
    for (i = 0; i < read_set->nb_entries; i++) {
        if (read_set->read_entries[i].address_shmem == address_shmem) {
            return TRUE;
        }
    }
#ifdef READDATATYPE
    read_set_insert(read_set, datatype, address_shmem);
#else
    read_set_insert(read_set, address_shmem);
#endif
    return FALSE;
}

inline read_entry_l_t * read_set_contains(read_set_t *read_set, void *address_shmem) {
    int i;
    for (i = read_set->nb_entries - 1; i >= 0; i--) {
        if (read_set->read_entries[i].address_shmem == address_shmem) {
            return &read_set->read_entries[i];
        }
    }

    return NULL;
}
