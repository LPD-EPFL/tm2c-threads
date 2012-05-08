#include "ssmp.h"

extern ssmp_msg_t *ssmp_mem;
extern ssmp_msg_t **ssmp_recv_buf;
extern ssmp_msg_t **ssmp_send_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t *ssmp_barrier;
static ssmp_msg_t *m;

/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void ssmp_send(int to, ssmp_msg_t *msg, int length) {
  m = ssmp_send_buf[to];
  while(m->state);      

  memcpy(m, msg, length);
  m->state = 1;

  PD("sent to %d", to);
}

inline void ssmp_sendb(int to, ssmp_msg_t *msg, int length) {
  
  while(ssmp_send_buf[to]->state);      

  memcpy(ssmp_send_buf[to], msg, length);

  ssmp_send_buf[to]->state = 1;
  while(ssmp_send_buf[to]->state);
  PD("sent to %d", to);
}


inline int ssmp_send_try(int to, ssmp_msg_t *msg, int length) {
  
  if (!ssmp_send_buf[to]->state) {

    memcpy(ssmp_send_buf[to], msg, length);

    ssmp_send_buf[to]->state = 1;

    return 1;
  }
  return 0;
}

/*
  specialized function for sending 1 to 6 words
 */

inline void ssmp_send1(int to, int w0) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  while(m->state);      

  m->w0 = w0;

  m->state = 1;

  PD("sent to %d", to);
}

inline void ssmp_send2(int to, int w0, int w1) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  while(m->state);      

  m->w0 = w0;
  m->w1 = w1;

  m->state = 1;

  PD("sent to %d", to);
}

inline void ssmp_send3(int to, int w0, int w1, int w2) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  while(m->state);      

  m->w0 = w0;
  m->w1 = w1;
  m->w2 = w2;

  m->state = 1;

  PD("sent to %d", to);
}

inline void ssmp_send4(int to, int w0, int w1, int w2, int w3) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  while(m->state);      

  m->w0 = w0;
  m->w1 = w1;
  m->w2 = w2;
  m->w3 = w3;

  m->state = 1;

  PD("sent to %d", to);
}

inline void ssmp_send5(int to, int w0, int w1, int w2, int w3, int w4) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  while(m->state);      

  m->w0 = w0;
  m->w1 = w1;
  m->w2 = w2;
  m->w3 = w3;
  m->w4 = w4;

  m->state = 1;

  PD("sent to %d", to);
}

inline void ssmp_send6(int to, int w0, int w1, int w2, int w3, int w4, int w5) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  while(m->state);      

  m->w0 = w0;
  m->w1 = w1;
  m->w2 = w2;
  m->w3 = w3;
  m->w4 = w4;
  m->w5 = w5;

  m->state = 1;

  PD("sent to %d", to);
}

inline int ssmp_send_try1(int to, int w0) { 
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  if (!m->state) {
    m->w0 = w0;

    m->state = 1;
    return 1;
  }
  return 0;
}

inline int ssmp_send_try2(int to, int w0, int w1) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  if (!m->state) {
    m->w0 = w0;
    m->w1 = w1;

    m->state = 1;
    return 1;
  }
  return 0;
} 

inline int ssmp_send_try3(int to, int w0, int w1, int w2) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  if (!m->state) {
    m->w0 = w0;
    m->w1 = w1;
    m->w2 = w2;

    m->state = 1;
    return 1;
  }
  return 0;
}

inline int ssmp_send_try4(int to, int w0, int w1, int w2, int w3) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  if (!m->state) {
    m->w0 = w0;
    m->w1 = w1;
    m->w2 = w2;
    m->w3 = w3;

    m->state = 1;
    return 1;
  }
  return 0;
}


inline int ssmp_send_try5(int to, int w0, int w1, int w2, int w3, int w4) { 
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  if (!m->state) {
    m->w0 = w0;
    m->w1 = w1;
    m->w2 = w2;
    m->w3 = w3;
    m->w4 = w4;

    m->state = 1;
    return 1;
  }
  return 0;
}

inline int ssmp_send_try6(int to, int w0, int w1, int w2, int w3, int w4, int w5) { 
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  if (!m->state) {
    m->w0 = w0;
    m->w1 = w1;
    m->w2 = w2;
    m->w3 = w3;
    m->w4 = w4;
    m->w5 = w5;

    m->state = 1;
    return 1;
  }
  return 0;
}
