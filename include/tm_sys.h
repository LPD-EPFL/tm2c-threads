#ifndef TM_SYS_H
#define TM_SYS_H

#include "tm_types.h"

/*
 * This method is called to initialize all necessary things
 * immediately upon startup
 */
void sys_init_system(int* argc, char** argv[]);

/*
 * This method is called by init_system to initialize the barrier
 */
void init_barrier();

/*
 * This method is called to terminate everything set up in init_system called
 */
void term_system();

/*
 * Allocate memory from the shared pool. Every node shares this memory.
 *
 * On RCCE that is the main shared memory. On cluster, we fake it...
 */

sys_t_vcharp sys_shmalloc(size_t);
void sys_shfree(sys_t_vcharp);

/*
 * Functions to deal with the number of nodes.
 */
INLINED nodeid_t NODE_ID(void);
INLINED nodeid_t TOTAL_NODES(void);

/*
 * Various helper initialization/termination functions
 * (sys_X is called from X initialization/termination function)
 */
void sys_tm_init(void);
void sys_ps_init_(void);
void sys_dsl_init(void);

void sys_dsl_term(void);
void sys_ps_term(void);

/*
 * Networking functions
 */
INLINED int sys_sendcmd(void* data, size_t len, nodeid_t to);
INLINED int sys_recvcmd(void* data, size_t len, nodeid_t from);

INLINED int sys_sendcmd_all(void* data, size_t len);

/*
 * to_intern_addr takes the address as used by the client, and transforms it to
 * the internal address (address recognized by the server code, that holds data).
 * to_addr does the opposite, takes the internal address, and transforms it to
 * the machine address.
 */
INLINED tm_intern_addr_t to_intern_addr(tm_addr_t addr);
INLINED tm_addr_t to_addr(tm_intern_addr_t i_addr);

/*
 * Random numbers related functions
 */
void srand_core();

/*
 * Helper functions
 */
void udelay(uint64_t micros);
void ndelay(uint64_t micros);

INLINED double wtime(void);

#endif
