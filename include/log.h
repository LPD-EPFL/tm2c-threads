/* 
 * File:   log.h
 * Author: trigonak
 *
 * Created on March 31, 2011, 2:27 PM
 * 
 * Read and Write logs for the TM
 */

#ifndef LOG_H
#define	LOG_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "common.h"
#ifdef PGAS
#include "pgas.h"
#endif

#define CAST_WORD(addr) *((unsigned int *) (addr))
#define CAST_CHAR(addr) *((char *) (addr))
#define CAST_UCHAR(addr) *((unsigned char *) (addr))
#define CAST_SHORT(addr) *((short int *) (addr))
#define CAST_USHORT(addr) *((unsigned short int  *) (addr))
#define CAST_INT(addr) *((int *) (addr))
#define CAST_UINT(addr) *((unsigned int *) (addr))
#define CAST_LONG(addr) *((long *) (addr))
#define CAST_LONGLONG(addr) *((long long *) (addr))
#define CAST_ULONG(addr) *((unsigned long *) (addr))
#define CAST_ULONGLONG(addr) *((unsigned long long *) (addr))
#define CAST_FLOAT(addr) *((float *) (addr))
#define CAST_DOUBLE(addr) *((double *) (addr))

    typedef enum {
        TYPE_POINTER = -13,
        TYPE_CHAR,
        TYPE_UCHAR,
        TYPE_SHORT,
        TYPE_USHORT,
        TYPE_INT,
        TYPE_UINT,
        TYPE_LONG,
        TYPE_LONGLONG,
        TYPE_ULONG,
        TYPE_ULONGLONG,
        TYPE_FLOAT,
        TYPE_DOUBLE
    } DATATYPE;


    /*______________________________________________________________________________________________________
     * WRITE SET                                                                                             |
     *______________________________________________________________________________________________________|
     */


#define WRITE_SET_SIZE 1024

    typedef struct write_entry {
        DATATYPE datatype; /* Data type */
        void *address_shmem;

        union {
            void *p;
            char c;
            unsigned char uc;
            short int s;
            unsigned short us;
            int i;
            unsigned int ui;
            long int li;
            long long int lli;
            unsigned long int uli;
            unsigned long long int ulli;
            float f;
            double d;
        };
    } write_entry_t;

    typedef struct write_set {
        write_entry_t *write_entries;
        unsigned int nb_entries;
        unsigned int size;
    } write_set_t;

    extern write_set_t * write_set_new();

    extern void write_set_free(write_set_t *write_set);

    extern write_set_t * write_set_empty(write_set_t *write_set);

    inline write_entry_t * write_set_entry(write_set_t *write_set);

    inline void write_entry_set_value(write_entry_t *we, void *value);

    extern void write_set_insert(write_set_t *write_set, DATATYPE datatype, void *value, void *address_shmem);

    extern void write_set_update(write_set_t *write_set, DATATYPE datatype, void *value, void *address_shmem);

    inline void write_entry_persist(write_entry_t *we);

    inline void write_entry_print(write_entry_t *we);

    extern void write_set_print(write_set_t *write_set);

    extern void write_set_persist(write_set_t *write_set);

    extern write_entry_t * write_set_contains(write_set_t *write_set, void *address_shmem);


    /*______________________________________________________________________________________________________
     * READ SET                                                                                             |
     *______________________________________________________________________________________________________|
     */

#define READ_SET_SIZE 1024

    typedef struct read_entry_l {
#ifdef READDATATYPE
        DATATYPE datatype;
#endif
        void *address_shmem;
    } read_entry_l_t;

    typedef struct read_set {
        read_entry_l_t *read_entries;
        unsigned int nb_entries;
        unsigned int size;
    } read_set_t;

    extern read_set_t * read_set_new();

    extern void read_set_free(read_set_t *read_set);

    extern read_set_t * read_set_empty(read_set_t *read_set);

    inline read_entry_l_t * read_set_entry(read_set_t *read_set);

#ifdef READDATATYPE

    extern void read_set_insert(read_set_t *read_set, DATATYPE datatype, void *address_shmem);
#else

    extern void read_set_insert(read_set_t *read_set, void *address_shmem);
#endif

#ifdef READDATATYPE

    extern BOOLEAN read_set_update(read_set_t *read_set, DATATYPE datatype, void *address_shmem);
#else

    extern BOOLEAN read_set_update(read_set_t *read_set, void *address_shmem);
#endif

    extern read_entry_l_t * read_set_contains(read_set_t *read_set, void *address_shmem);

#ifdef	__cplusplus
}
#endif


#ifdef PGAS

/*______________________________________________________________________________________________________
 * WRITE SET PGAS                                                                                       |
 *______________________________________________________________________________________________________|
 */


#define WRITE_SET_PGAS_SIZE     384

typedef struct write_entry_pgas {
    unsigned int address;
    int value;
} write_entry_pgas_t;

typedef struct write_set_pgas {
    write_entry_pgas_t *write_entries;
    unsigned int nb_entries;
    unsigned int size;
} write_set_pgas_t;

extern write_set_pgas_t * write_set_pgas_new();

extern void write_set_pgas_free(write_set_pgas_t *write_set_pgas);

extern write_set_pgas_t * write_set_pgas_empty(write_set_pgas_t *write_set_pgas);

inline write_entry_pgas_t * write_set_pgas_entry(write_set_pgas_t *write_set_pgas);

extern void write_set_pgas_insert(write_set_pgas_t *write_set_pgas, int value, unsigned int address);

extern void write_set_pgas_update(write_set_pgas_t *write_set_pgas, int value, unsigned int address);

inline void write_entry_pgas_persist(write_entry_pgas_t *we);

inline void write_entry_pgas_print(write_entry_pgas_t *we);

extern void write_set_pgas_print(write_set_pgas_t *write_set_pgas);

extern void write_set_pgas_persist(write_set_pgas_t *write_set_pgas);

extern write_entry_pgas_t * write_set_pgas_contains(write_set_pgas_t *write_set_pgas, unsigned int address);


#endif

#endif	/* LOG_H */

