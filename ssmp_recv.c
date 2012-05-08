#include "ssmp.h"

extern ssmp_msg_t *ssmp_mem;
extern ssmp_msg_t **ssmp_recv_buf;
extern ssmp_msg_t **ssmp_send_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t *ssmp_barrier;
static ssmp_msg_t *tmpm;

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void ssmp_recv_from(int from, ssmp_msg_t *msg, int length) {
  tmpm = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  while(!tmpm->state);

  memcpy(msg, tmpm, length);
  tmpm->state = 0;

  msg->sender = from;

  PD("recved from %d\n", from);
}

inline int ssmp_recv_from_try(int from, ssmp_msg_t *msg, int length) {
  PD("recv from %d\n", from);
  if (ssmp_recv_buf[from]->state) {

    memcpy(msg, ssmp_recv_buf[from], length);

    msg->sender = from;
    ssmp_recv_buf[from]->state = 0;
    return 1;
  }
  return 0;
}


inline void ssmp_recv(ssmp_msg_t *msg, int length) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try(last_recv_from, msg, length)) {
      return;
    }
  }
}

inline int ssmp_recv_try(ssmp_msg_t *msg, int length) {
  int i;
  for (i = 0; i < ssmp_num_ues_; i++) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try(last_recv_from, msg, length)) {
      return 1;
    }
  }

  return 0;
}


inline void ssmp_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg, int length) {
  int from;
  while(1) {
    //XXX: maybe have a last_recv_from field
    for (from = 0; from < cbuf->num_ues; from++) {

      if (cbuf->buf[from]->state) {
	memcpy(msg, cbuf->buf[from], length);

	msg->sender = cbuf->from[from];
	cbuf->buf[from]->state = 0;
	return;
      }
    }
  }
}


inline void ssmp_recv_from4(int from, ssmp_msg_t *msg) {
  ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  while(!m->state);

  msg->sender = from;
  msg->w0 = m->w0;
  msg->w1 = m->w1;
  msg->w2 = m->w2;
  msg->w3 = m->w3;

  m->state = 0;
  PD("recved from %d\n", from);
}

inline void ssmp_recv_from6(int from, ssmp_msg_t *msg) {
  ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  while(!m->state);


  msg->w0 = m->w0;
  msg->w1 = m->w1;
  msg->w2 = m->w2;
  msg->w3 = m->w3;
  msg->w4 = m->w4;
  msg->w5 = m->w5;
  msg->sender = from;

  m->state = 0;
  PD("recved from %d\n", from);
}



inline int ssmp_recv_from_try1(int from, ssmp_msg_t *msg) {
  ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  if (m->state) {

    msg->sender = from;
    msg->w0 = m->w0;

    m->state = 0;
    PD("recved from %d\n", from);
    return 1;
  }
  return 0;
}

inline int ssmp_recv_from_try4(int from, ssmp_msg_t *msg) {
  ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  if (m->state) {

    msg->sender = from;
    msg->w0 = m->w0;
    msg->w1 = m->w1;
    msg->w2 = m->w2;
    msg->w3 = m->w3;

    m->state = 0;
    PD("recved from %d\n", from);
    return 1;
  }
  return 0;
}


inline int ssmp_recv_from_try6(int from, ssmp_msg_t *msg) {
  ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  if (m->state) {

    msg->sender = from;
    msg->w0 = m->w0;
    msg->w1 = m->w1;
    msg->w2 = m->w2;
    msg->w3 = m->w3;
    msg->w4 = m->w4;
    msg->w5 = m->w5;

    m->state = 0;
    PD("recved from %d\n", from);
    return 1;
  }
  return 0;
}



inline void ssmp_recv1(ssmp_msg_t *msg) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try1(last_recv_from, msg)) {
      return;
    }
  }
}


inline void ssmp_recv4(ssmp_msg_t *msg) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try4(last_recv_from, msg)) {
      return;
    }
  }
}


inline void ssmp_recv6(ssmp_msg_t *msg) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try6(last_recv_from, msg)) {
      return;
    }
  }
}


inline int ssmp_recv_try4(ssmp_msg_t *msg) {
  int i;
  for (i = 0; i < ssmp_num_ues_; i++) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try4(last_recv_from, msg)) {
      return 1;
    }
  }

  return 0;
}

inline int ssmp_recv_try6(ssmp_msg_t *msg) {
  int i;
  for (i = 0; i < ssmp_num_ues_; i++) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try6(last_recv_from, msg)) {
      return 1;
    }
  }

  return 0;
}

inline void ssmp_recv_color4(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg) {
  int from;
  while(1) {
    //XXX: maybe have a last_recv_from field
    for (from = 0; from < cbuf->num_ues; from++) {

      if (cbuf->buf[from]->state) {
	msg->w0 = cbuf->buf[from]->w0;
	msg->w1 = cbuf->buf[from]->w1;
	msg->w2 = cbuf->buf[from]->w2;
	msg->w3 = cbuf->buf[from]->w3;

	msg->sender = cbuf->from[from];
	cbuf->buf[from]->state = 0;
	return;
      }
    }
  }
}

inline void ssmp_recv_color6(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg) {
  int from;
  while(1) {
    //XXX: maybe have a last_recv_from field
    for (from = 0; from < cbuf->num_ues; from++) {

      if (cbuf->buf[from]->state) {
	msg->w0 = cbuf->buf[from]->w0;
	msg->w1 = cbuf->buf[from]->w1;
	msg->w2 = cbuf->buf[from]->w2;
	msg->w3 = cbuf->buf[from]->w3;
	msg->w4 = cbuf->buf[from]->w4;
	msg->w5 = cbuf->buf[from]->w5;

	msg->sender = cbuf->from[from];
	cbuf->buf[from]->state = 0;
	return;
      }
    }
  }
}

