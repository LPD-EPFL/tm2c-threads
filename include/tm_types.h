#ifndef TM_TYPES_H
#define TM_TYPES_H

#include <inttypes.h>
#include <stdint.h>

#if SIZE_MAX == 4294967295
#define PRIxSIZE PRIx32
#elif SIZE_MAX > 4294967295
#define PRIxSIZE PRIx64
#else
#define PRIxSIZE x
#endif

typedef enum {
	FALSE, //0
	TRUE //1
} BOOLEAN;

typedef uint32_t nodeid_t;
typedef void* tm_addr_t;

/*
 * The format in printf to represent tm_addr_t
 */
#define PRIxA "p"

#ifdef PGAS
/*
 * With PGAS, addresses that are handled by the dslock nodes are offsets
 */
typedef size_t tm_intern_addr_t;

/*
 * The format in printf to represent this address
 */
#define PRIxIA "#0" PRIxSIZE 
#else
/*
 * However, for the non-PGAS code, they are uintptr_t 
 */
typedef uintptr_t tm_intern_addr_t;
#define PRIxIA PRIxPTR
#endif

/*
 * Type for holding the pointers when dealing with memory
 * XXX: needs fixing, maybe removal
 */
typedef volatile unsigned char* sys_t_vcharp;

#endif
