#ifndef TM_SYS_H
#define TM_SYS_H

/*
 * This method is called to initialize all necessary things
 * immediately upon startup
 */
void init_system(int* argc, char** argv[]);

/*
 * This method is called to terminate everything set up in init_system called
 */
void term_system();

/*
 * Allocate memory from the shared pool. Every node shares this memory.
 *
 * On RCCE that is the main shared memory. On cluster, we fake it...
 */

typedef volatile unsigned char* sys_t_vcharp;
sys_t_vcharp sys_shmalloc(size_t);
void sys_shfree(sys_t_vcharp);

#endif
