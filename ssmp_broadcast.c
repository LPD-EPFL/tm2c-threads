#include "ssmp.h"

extern ssmp_msg_t *ssmp_mem;
extern ssmp_msg_t **ssmp_recv_buf;
extern ssmp_msg_t **ssmp_send_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t *ssmp_barrier;


/* ------------------------------------------------------------------------------- */
/* broadcasting functions */
/* ------------------------------------------------------------------------------- */

inline void ssmp_broadcast(ssmp_msg_t *msg, int length) {
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send(core, msg, length);
  }
}

inline void ssmp_broadcast1(int w0) { 
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send1(core, w0);
  }
}

inline void ssmp_broadcast2(int w0, int w1) { 
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send2(core, w0, w1);
  }
}

inline void ssmp_broadcast3(int w0, int w1, int w2) { 
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send3(core, w0, w1, w2);
  }
}

inline void ssmp_broadcast4(int w0, int w1, int w2, int w3) {
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send4(core, w0, w1, w2, w3);
  }
}


inline void ssmp_broadcast5(int w0, int w1, int w2, int w3, int w4) { 
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send5(core, w0, w1, w2, w3, w4);
  }
}

inline void ssmp_broadcast6(int w0, int w1, int w2, int w3, int w4, int w5) { 
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send6(core, w0, w1, w2, w3, w4, w5);
  }
}

inline void ssmp_broadcast_par(int w0, int w1, int w2, int w3) {
  int *sent = (int *) calloc(ssmp_num_ues_, sizeof(int));
  int num_to_send = ssmp_num_ues_ - 1;
  int try_to = ssmp_id_;
  while (num_to_send > 0) {
    try_to = (try_to + 1) % ssmp_num_ues_;
    if (try_to == ssmp_num_ues_ || sent[try_to]) {
      continue;
    }
    
    if (ssmp_send_try4(try_to, w0, w1, w2, w3)) {
      num_to_send--;
      sent[try_to] = 1;
    }
  }

  free(sent);
}
