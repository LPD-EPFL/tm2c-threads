#ifndef TM_TYPES_H
#define TM_TYPES_H

#include <inttypes.h>
#include <stdint.h>

typedef enum {
	FALSE, //0
	TRUE //1
} BOOLEAN;

typedef unsigned int nodeid_t;
typedef void* tm_addr_t;

#ifdef PGAS
/*
 * With PGAS, addresses that are handled by the dslock nodes are offsets
 */
typedef size_t tm_intern_addr_t;
#else
/*
 * However, for the non-PGAS code, they are uintptr_t 
 */
typedef uintptr_t tm_intern_addr_t;
#endif

/*
 * Type for holding the pointers when dealing with memory
 * XXX: needs fixing, maybe removal
 */
typedef volatile unsigned char* sys_t_vcharp;

#endif
