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

inline write_set_t * write_set_empty(write_set_t *write_set) {

    if (write_set->size > WRITE_SET_SIZE) {
        write_entry_t * temp;
        if ((temp = (write_entry_t *) realloc(write_set->write_entries, WRITE_SET_SIZE * sizeof (write_entry_t))) == NULL) {
            free(write_set->write_entries);
            PRINT("realloc @ write_set_empty failed");
            write_set->write_entries = (write_entry_t *) malloc(WRITE_SET_SIZE * sizeof (write_entry_t));
            if (write_set->write_entries == NULL) {
                PRINT("malloc write_set->write_entries @ write_set_empty");
                return NULL;
            }
        }
    }
    write_set->size = WRITE_SET_SIZE;
    write_set->nb_entries = 0;
    return write_set;
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

inline void write_set_insert(write_set_t *write_set, DATATYPE datatype, void *value, tm_intern_addr_t address) {
    write_entry_t *we = write_set_entry(write_set);

    we->datatype = datatype;
    we->address = address;
    write_entry_set_value(we, value);
}

inline void write_set_update(write_set_t *write_set, DATATYPE datatype, void *value, tm_intern_addr_t address) {
    unsigned int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        if (write_set->write_entries[i].address == address) {
            write_entry_set_value(&write_set->write_entries[i], value);
            return;
        }
    }

    write_set_insert(write_set, datatype, value, address);
}

/*
 * This method should persist the data. Hence, we need to write at the
 * location represented by .address this particular data type.
 * To do so, we need to translate this address from the tm_intern_addr_t to
 * tm_addr_t on the system.
 */
inline void write_entry_persist(write_entry_t *we) {
	tm_addr_t shmem_address = to_addr(we->address);
    switch (we->datatype) {
        case TYPE_CHAR:
            CAST_CHAR(shmem_address) = we->c;
            break;
        case TYPE_DOUBLE:
            CAST_DOUBLE(shmem_address) = we->d;
            break;
        case TYPE_FLOAT:
            CAST_FLOAT(shmem_address) = we->f;
            break;
        case TYPE_INT:
            CAST_INT(shmem_address) = we->i;
            break;
        case TYPE_LONG:
            CAST_LONG(shmem_address) = we->li;
            break;
        case TYPE_LONGLONG:
            CAST_LONGLONG(shmem_address) = we->lli;
            break;
        case TYPE_SHORT:
            CAST_SHORT(shmem_address) = we->s;
            break;
        case TYPE_UCHAR:
            CAST_UCHAR(shmem_address) = we->uc;
            break;
        case TYPE_UINT:
            CAST_UINT(shmem_address) = we->ui;
            break;
        case TYPE_ULONG:
            CAST_ULONG(shmem_address) = we->uli;
            break;
        case TYPE_ULONGLONG:
            CAST_ULONGLONG(shmem_address) = we->ulli;
            break;
        case TYPE_USHORT:
            CAST_USHORT(shmem_address) = we->us;
            break;
        case TYPE_POINTER:
            *(tm_intern_addr_t*)shmem_address = (tm_intern_addr_t)we->p;
            break;
        default:
            /* this is not safe */
            memcpy(&shmem_address, we->p, we->datatype);
    }
}

inline void write_entry_print(write_entry_t *we) {
    switch (we->datatype) {
        case TYPE_CHAR:
            PRINTSME("[%"PRIxIA" :  %c]", (we->address), we->c);
            break;
        case TYPE_DOUBLE:
            PRINTSME("[%"PRIxIA" :  %f]", (we->address), we->d);
            break;
        case TYPE_FLOAT:
            PRINTSME("[%"PRIxIA" :  %f]", (we->address), we->f);
            break;
        case TYPE_INT:
            PRINTSME("[%"PRIxIA" :  %d]", (we->address), we->i);
            break;
        case TYPE_LONG:
            PRINTSME("[%"PRIxIA" :  %ld]", (we->address), we->li);
            break;
        case TYPE_LONGLONG:
            PRINTSME("[%"PRIxIA" :  %lld]", (we->address), we->lli);
            break;
        case TYPE_SHORT:
            PRINTSME("[%"PRIxIA" :  %i]", (we->address), we->s);
            break;
        case TYPE_UCHAR:
            PRINTSME("[%"PRIxIA" :  %c]", (we->address), we->uc);
            break;
        case TYPE_UINT:
            PRINTSME("[%"PRIxIA" :  %u]", (we->address), we->ui);
            break;
        case TYPE_ULONG:
            PRINTSME("[%"PRIxIA" :  %lu]", (we->address), we->uli);
            break;
        case TYPE_ULONGLONG:
            PRINTSME("[%"PRIxIA" :  %llu]", (we->address), we->ulli);
            break;
        case TYPE_USHORT:
            PRINTSME("[%"PRIxIA" :  %us]", (we->address), we->us);
            break;
        case TYPE_POINTER:
            PRINTSME("[%"PRIxIA" :  %p]", we->address, we->p);
            break;
        default:
            PRINTSME("[%"PRIxIA" :  %s]", we->address, (const char *) we->p);
    }
}

inline void write_set_print(write_set_t *write_set) {
    PRINTSME("WRITE SET (elements: %d, size: %d) --------------\n", write_set->nb_entries, write_set->size);
    unsigned int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        write_entry_print(&write_set->write_entries[i]);
    }
    FLUSH
}

inline void write_set_persist(write_set_t *write_set) {
    unsigned int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        write_entry_persist(&write_set->write_entries[i]);
    }
}

inline write_entry_t * write_set_contains(write_set_t *write_set, tm_intern_addr_t address) {
  uint32_t i;
  for (i = 0; i < write_set->nb_entries; i++)
    {
      write_entry_t* wes = write_set->write_entries;
      if (wes[i].address == address) 
	{
	  return &wes[i];
	}
    }

  return NULL;
}


#ifdef PGAS

/*______________________________________________________________________________________________________
 * WRITE SET PGAS                                                                                       |
 *______________________________________________________________________________________________________|
 */


inline write_set_pgas_t * write_set_pgas_new() {
    write_set_pgas_t *write_set_pgas;

    if ((write_set_pgas = (write_set_pgas_t *) malloc(sizeof (write_set_pgas_t))) == NULL) {
        PRINT("Could not initialize the write set");
        return NULL;
    }

    if ((write_set_pgas->write_entries = (write_entry_pgas_t *) malloc(WRITE_SET_PGAS_SIZE * sizeof (write_entry_pgas_t))) == NULL) {
        free(write_set_pgas);
        PRINT("Could not initialize the write set");
        return NULL;
    }

    write_set_pgas->nb_entries = 0;
    write_set_pgas->size = WRITE_SET_PGAS_SIZE;

    return write_set_pgas;
}

inline void write_set_pgas_free(write_set_pgas_t *write_set_pgas) {
    free(write_set_pgas->write_entries);
    free(write_set_pgas);
}

inline write_set_pgas_t * write_set_pgas_empty(write_set_pgas_t *write_set_pgas) {

#ifdef WRITE_SET_RESIZE
    if (write_set_pgas->size > WRITE_SET_PGAS_SIZE) {
        write_entry_pgas_t * temp;
        if ((temp = (write_entry_pgas_t *) realloc(write_set_pgas->write_entries, WRITE_SET_PGAS_SIZE * sizeof (write_entry_pgas_t))) == NULL) {
            free(write_set_pgas->write_entries);
            PRINT("realloc @ write_set_pgas_empty failed");
            write_set_pgas->write_entries = (write_entry_pgas_t *) malloc(WRITE_SET_PGAS_SIZE * sizeof (write_entry_pgas_t));
            if (write_set_pgas->write_entries == NULL) {
                PRINT("malloc write_set_pgas->write_entries @ write_set_pgas_empty");
                return NULL;
            }
        }
    }
    write_set_pgas->size = WRITE_SET_PGAS_SIZE;
#endif
    write_set_pgas->nb_entries = 0;
    return write_set_pgas;
}

inline write_entry_pgas_t * write_set_pgas_entry(write_set_pgas_t *write_set_pgas) {
    if (write_set_pgas->nb_entries == write_set_pgas->size) {
        unsigned int new_size = 2 * write_set_pgas->size;
        PRINT("WRITE set max sized (%d)(%d)", write_set_pgas->size, new_size);
        write_entry_pgas_t *temp;
        if ((temp = (write_entry_pgas_t *) realloc(write_set_pgas->write_entries, new_size * sizeof (write_entry_pgas_t))) == NULL) {
            PRINT("Could not resize the write set pgas");
            write_set_pgas_free(write_set_pgas);
            return NULL;
        }

        write_set_pgas->write_entries = temp;
        write_set_pgas->size = new_size;
    }
    
    return &write_set_pgas->write_entries[write_set_pgas->nb_entries++];
}

inline void write_set_pgas_insert(write_set_pgas_t *write_set_pgas, int value, tm_intern_addr_t address) {
    write_entry_pgas_t *we = write_set_pgas_entry(write_set_pgas);

    we->address = address;
    we->value = value;
}

inline void write_set_pgas_update(write_set_pgas_t *write_set_pgas, int value, tm_intern_addr_t address) {
    unsigned int i;
    for (i = 0; i < write_set_pgas->nb_entries; i++) {
        if (write_set_pgas->write_entries[i].address == address) {
            write_set_pgas->write_entries[i].value = value;
            return;
        }
    }

    write_set_pgas_insert(write_set_pgas, value, address);
}

inline void write_entry_pgas_persist(write_entry_pgas_t *we) {
    PGAS_write(we->address, we->value);
}

inline void write_entry_pgas_print(write_entry_pgas_t *we) {
    PRINTSME("[%5d :  %d]", (we->address), we->value);
}

inline void write_set_pgas_print(write_set_pgas_t *write_set_pgas) {
    PRINTSME("WRITE SET PGAS (elements: %d, size: %d) --------------\n", write_set_pgas->nb_entries, write_set_pgas->size);
    unsigned int i;
    for (i = 0; i < write_set_pgas->nb_entries; i++) {
        write_entry_pgas_print(&write_set_pgas->write_entries[i]);
    }
    FLUSH
}

inline void write_set_pgas_persist(write_set_pgas_t *write_set_pgas) {
    unsigned int i;
    for (i = 0; i < write_set_pgas->nb_entries; i++) {
        write_entry_pgas_persist(&write_set_pgas->write_entries[i]);
    }
}

inline write_entry_pgas_t* write_set_pgas_contains(write_set_pgas_t *write_set_pgas, tm_intern_addr_t address) {
    unsigned int i;
    for (i = write_set_pgas->nb_entries; i-- > 0;) {
        if (write_set_pgas->write_entries[i].address == address) {
            return &write_set_pgas->write_entries[i];
        }
    }

    return NULL;
}

#endif
