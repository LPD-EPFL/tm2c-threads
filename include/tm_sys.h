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

/*
 * Various helper initialization functions
 * (sys_X is called from X initialization function)
 */
void sys_tm_init(unsigned int ID);
void sys_ps_init_(void);
void sys_dsl_init(void);

/*
 * Networking functions
 */
int sys_sendcmd(void* data, size_t len, nodeid_t target);
int sys_recvcmd(void* data, size_t len, nodeid_t target);
#endif
