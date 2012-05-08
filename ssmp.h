#ifndef _SSMP_H_
#define _SSMP_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>

/* ------------------------------------------------------------------------------- */
/* defines */
/* ------------------------------------------------------------------------------- */
#define SSMP_NUM_BARRIERS 16 /*number of available barriers*/

#define SP(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#ifdef SSMP_DEBUG
#define PD(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#else
#define PD(args...) 
#endif


/* ------------------------------------------------------------------------------- */
/* types */
/* ------------------------------------------------------------------------------- */
typedef int ssmp_chk_t; /*used for the checkpoints*/

/*msg type: contains 15 words of data and 1 word flag*/
typedef struct {
  int w0;
  int w1;
  int w2;
  int w3;
  int w4;
  int w5;
  int w6;
  int f[8];
union {
    int state;
    int sender;
  };
} ssmp_msg_t;

/*type used for color-based function, i.e. functions that operate on a subset of the cores according to a color function*/
typedef struct {
  int num_ues;
  ssmp_msg_t **buf;
  int *from;
} ssmp_color_buf_t;


/*barrier type*/
typedef struct {
  long long unsigned int participants; /*the participants of a barrier can be given either by this, as bits (0 -> no, 1 ->participate */
  int (*color)(int); /*or as a color function: if the function return 0 -> no participant, 1 -> participant. The color function has priority over the lluint participants*/
  ssmp_chk_t * checkpoints; /*the checkpoints array used for sync*/
  unsigned int version; /*the current version of the barrier, used to make a barrier reusable*/
} ssmp_barrier_t;


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

/* initialize the system: called before forking */
extern void ssmp_init(int num_procs);
/* initilize the memory structures of the system: called after forking by every proc */
extern void ssmp_mem_init(int id, int num_ues);
/* terminate the system */
extern void ssmp_term(void);

/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking send length words to to */
/* blocking in the sense that the data are copied to the receiver's buffer */
extern inline void ssmp_send(int to, ssmp_msg_t *msg, int length);
/* blocking until the receivers reads the data */
extern inline void ssmp_sendb(int to, ssmp_msg_t *msg, int length);
/* blocking send from 1 to 6 words to to */
extern inline void ssmp_send1(int to, int w0);
extern inline void ssmp_send2(int to, int w0, int w1);
extern inline void ssmp_send3(int to, int w0, int w1, int w2);
extern inline void ssmp_send4(int to, int w0, int w1, int w2, int w3);
extern inline void ssmp_send5(int to, int w0, int w1, int w2, int w3, int w4);
extern inline void ssmp_send6(int to, int w0, int w1, int w2, int w3, int w4, int w5);

/* non-blocking send length words
   returns 1 if successful, 0 if not */
extern inline int ssmp_send_try(int to, ssmp_msg_t *msg, int length);
/* non-blocking send from 1 to 6 words */
extern inline int ssmp_send_try1(int to, int w0);
extern inline int ssmp_send_try2(int to, int w0, int w1);
extern inline int ssmp_send_try3(int to, int w0, int w1, int w2);
extern inline int ssmp_send_try4(int to, int w0, int w1, int w2, int w3);
extern inline int ssmp_send_try5(int to, int w0, int w1, int w2, int w3, int w4);
extern inline int ssmp_send_try6(int to, int w0, int w1, int w2, int w3, int w4, int w5);


/* ------------------------------------------------------------------------------- */
/* broadcasting functions */
/* ------------------------------------------------------------------------------- */

/* broadcast length bytes using blocking sends */
extern inline void ssmp_broadcast(ssmp_msg_t *msg, int length);
/* broadcast 1 to 7 words using blocking sends */
extern inline void ssmp_broadcast1(int w0);
extern inline void ssmp_broadcast2(int w0, int w1);
extern inline void ssmp_broadcast3(int w0, int w1, int w2);
extern inline void ssmp_broadcast4(int w0, int w1, int w2, int w3);
extern inline void ssmp_broadcast5(int w0, int w1, int w2, int w3, int w4);
extern inline void ssmp_broadcast6(int w0, int w1, int w2, int w3, int w4, int w5);
extern inline void ssmp_broadcast7(int w0, int w1, int w2, int w3, int w4, int w5, int w6);
/* broadcast 1 to 7 words using non-blocking sends (~parallelize sends) */
extern inline void ssmp_broadcast_par(int w0, int w1, int w2, int w3); //XXX: fix perf

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking receive from process from length bytes */
extern inline void ssmp_recv_from(int from, ssmp_msg_t *msg, int length);
/* blocking receive from process from 6 bytes */
extern inline void ssmp_recv_from6(int from, ssmp_msg_t *msg);
/* non-blocking receive from process from
   returns 1 if recved msg, else 0 */
extern inline int ssmp_recv_from_try(int from, ssmp_msg_t *msg, int length);
extern inline int ssmp_recv_from_try1(int from, ssmp_msg_t *msg);
extern inline int ssmp_recv_from_try6(int from, ssmp_msg_t *msg);
/* blocking receive from any proc. 
   Sender at msg->sender */
extern inline void ssmp_recv(ssmp_msg_t *msg, int length);
extern inline void ssmp_recv1(ssmp_msg_t *msg);
extern inline void ssmp_recv6(ssmp_msg_t *msg);
/* blocking receive from any proc. 
   returns 1 if recved msg, else 0
   Sender at msg->sender */
extern inline int ssmp_recv_try(ssmp_msg_t *msg, int length);
extern inline int ssmp_recv_try6(ssmp_msg_t *msg);


/* ------------------------------------------------------------------------------- */
/* color-based recv fucntions */
/* ------------------------------------------------------------------------------- */

/* initialize the color buf data structure to be used with consequent ssmp_recv_color calls. A node is considered a participant if the call to color(ID) returns 1 */
extern inline void ssmp_color_buf_init(ssmp_color_buf_t *cbuf, int (*color)(int));
extern inline void ssmp_color_buf_free(ssmp_color_buf_t *cbuf);

/* blocking receive from any of the participants according to the color function */
extern inline void ssmp_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg, int length);
extern inline void ssmp_recv_color4(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);
extern inline void ssmp_recv_color6(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);

/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */
extern int color_app(int id);

extern inline ssmp_barrier_t * ssmp_get_barrier(int barrier_num);
extern inline void ssmp_barrier_init(int barrier_num, long long int participants, int (*color)(int));

extern inline void ssmp_barrier_wait(int barrier_num);


/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */
extern inline double wtime(void);
extern void set_cpu(int cpu);

typedef uint64_t ticks;
extern inline ticks getticks(void);

extern inline int ssmp_id();
extern inline int ssmp_num_ues();

#endif
