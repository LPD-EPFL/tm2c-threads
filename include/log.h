/* 
 * File:   log.h
 * Author: trigonak
 *
 * Created on March 31, 2011, 2:27 PM
 * 
 * Read and Write logs for the TM
 * As such, they need to keep the internal representation of the addresses
 * (tm_intern_addr_t).
 * In EAGER_WRITE_ACQ PGAS, only the dslock keeps the buffer.
 *      XXX: since only the dslock keeps the buffer, when an app node is performing
 *              a read, the serving dslock core should test if the value exists in the
 *              write-buffer of the requester. THIS IS NOT IMPLEMENTED FOR PERFORMANCE,
 *              but could lead to breaking opacity
 * In non-EAGER_WRITE_ACQ PGAS, both sides keep the buffer.
 * In other configurations, both sides keep the buffer.
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
        tm_intern_addr_t address;

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

    extern void write_set_insert(write_set_t *write_set, DATATYPE datatype, void *value, tm_intern_addr_t address);

    extern void write_set_update(write_set_t *write_set, DATATYPE datatype, void *value, tm_intern_addr_t address);

    inline void write_entry_persist(write_entry_t *we);

    inline void write_entry_print(write_entry_t *we);

    extern void write_set_print(write_set_t *write_set);

    extern void write_set_persist(write_set_t *write_set);

    extern write_entry_t * write_set_contains(write_set_t *write_set, tm_intern_addr_t address);

#ifdef PGAS

/*______________________________________________________________________________________________________
 * WRITE SET PGAS                                                                                       |
 *______________________________________________________________________________________________________|
 */


#define WRITE_SET_PGAS_SIZE     384

typedef struct write_entry_pgas {
    tm_intern_addr_t address;
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

extern void write_set_pgas_insert(write_set_pgas_t *write_set_pgas, int value, tm_intern_addr_t address);

extern void write_set_pgas_update(write_set_pgas_t *write_set_pgas, int value, tm_intern_addr_t address);

inline void write_entry_pgas_persist(write_entry_pgas_t *we);

inline void write_entry_pgas_print(write_entry_pgas_t *we);

extern void write_set_pgas_print(write_set_pgas_t *write_set_pgas);

extern void write_set_pgas_persist(write_set_pgas_t *write_set_pgas);

extern write_entry_pgas_t * write_set_pgas_contains(write_set_pgas_t *write_set_pgas, tm_intern_addr_t address);


#endif

#endif	/* LOG_H */

