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
#define SSMP_NUM_BARRIERS 16


/* ------------------------------------------------------------------------------- */
/* types */
/* ------------------------------------------------------------------------------- */
typedef int ssmp_chk_t;

typedef struct {
  union {
    int state;
    int sender;
  };
  int w0;
  int w1;
  int w2;
  int w3;
  int w4;
  int w5;
  int w6;
  //  int f[8];
} ssmp_msg_t;

typedef struct {
  int num_ues;
  ssmp_msg_t **buf;
  int *from;
} ssmp_color_buf_t;


typedef struct {
  long long unsigned int participants;
  int (*color)(int);
  ssmp_chk_t * checkpoints;
  unsigned int version;
} ssmp_barrier_t;


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */
extern void ssmp_init(int num_procs);
extern void ssmp_mem_init(int id, int num_ues);

extern void ssmp_term(void);

/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking send from 1 to 7 words */
extern inline void ssmp_send(int to, int w0, int w1, int w2, int w3);
extern inline void ssmp_send1(int to, int w0);
extern inline void ssmp_send2(int to, int w0, int w1);
extern inline void ssmp_send3(int to, int w0, int w1, int w2);
#define ssmp_send4 ssmp_send
extern inline void ssmp_send5(int to, int w0, int w1, int w2, int w3, int w4);
extern inline void ssmp_send6(int to, int w0, int w1, int w2, int w3, int w4, int w5);
extern inline void ssmp_send7(int to, int w0, int w1, int w2, int w3, int w4, int w5, int w6);
/* non-blocking send from 1 to 7 words */
extern inline int ssmp_send_try(int to, int w0, int w1, int w2, int w3);
extern inline int ssmp_send_try1(int to, int w0);
extern inline int ssmp_send_try2(int to, int w0, int w1);
extern inline int ssmp_send_try3(int to, int w0, int w1, int w2);
#define ssmp_send_try4  ssmp_send_try
extern inline int ssmp_send_try5(int to, int w0, int w1, int w2, int w3, int w4);
extern inline int ssmp_send_try6(int to, int w0, int w1, int w2, int w3, int w4, int w5);
extern inline int ssmp_send_try7(int to, int w0, int w1, int w2, int w3, int w4, int w5, int w6);

/* ------------------------------------------------------------------------------- */
/* broadcasting functions */
/* ------------------------------------------------------------------------------- */

/* broadcast 1 to 7 words using blocking sends */
extern inline void ssmp_broadcast(int w0, int w1, int w2, int w3);
extern inline void ssmp_broadcast1(int w0);
extern inline void ssmp_broadcast2(int w0, int w1);
extern inline void ssmp_broadcast3(int w0, int w1, int w2);
#define ssmp_broadcast4  ssmp_broadcast
extern inline void ssmp_broadcast5(int w0, int w1, int w2, int w3, int w4);
extern inline void ssmp_broadcast6(int w0, int w1, int w2, int w3, int w4, int w5);
extern inline void ssmp_broadcast7(int w0, int w1, int w2, int w3, int w4, int w5, int w6);
/* broadcast 1 to 7 words using non-blocking sends (~parallelize sends) */
extern inline void ssmp_broadcast_par(int w0, int w1, int w2, int w3); //XXX: fix perf

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking receive from process from */
extern inline void ssmp_recv_from(int from, ssmp_msg_t *msg);
extern inline void ssmp_recv_from6(int from, ssmp_msg_t *msg);
/* non-blocking receive from process from
   returns 1 if recved msg, else 0 */
extern inline int ssmp_recv_from_try(int from, ssmp_msg_t *msg);
extern inline int ssmp_recv_from_try1(int from, ssmp_msg_t *msg);
extern inline int ssmp_recv_from_try6(int from, ssmp_msg_t *msg);
/* blocking receive from any proc. 
   Sender at msg->sender */
extern inline void ssmp_recv(ssmp_msg_t *msg);
extern inline void ssmp_recv1(ssmp_msg_t *msg);
extern inline void ssmp_recv6(ssmp_msg_t *msg);
/* blocking receive from any proc. 
   returns 1 if recved msg, else 0
   Sender at msg->sender */
extern inline int ssmp_recv_try(ssmp_msg_t *msg);
extern inline int ssmp_recv_try6(ssmp_msg_t *msg);


/* ------------------------------------------------------------------------------- */
/* color-based recv fucntions */
/* ------------------------------------------------------------------------------- */

extern inline void ssmp_color_buf_init(ssmp_color_buf_t *cbuf, int (*color)(int));
extern inline void ssmp_color_buf_free(ssmp_color_buf_t *cbuf);
extern inline void ssmp_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);

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
