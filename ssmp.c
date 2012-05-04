#include "ssmp.h"

//#define SSMP_DEBUG

#define SP(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#ifdef SSMP_DEBUG
#define PD(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#else
#define PD(args...) 
#endif


/* ------------------------------------------------------------------------------- */
/* library variables */
/* ------------------------------------------------------------------------------- */

ssmp_msg_t *ssmp_mem;
ssmp_msg_t **ssmp_recv_buf;
ssmp_msg_t **ssmp_send_buf;
int ssmp_num_ues_;
int ssmp_id_;
int last_recv_from;
ssmp_barrier_t *ssmp_barrier;
int *ues_initialized;


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

void ssmp_init(int num_procs)
{
  //create the shared space which will be managed by the allocator
  int sizem, sizeb, sizeckp, sizeui, size;

  sizem = (num_procs * num_procs) * sizeof(ssmp_msg_t);
  sizeb = SSMP_NUM_BARRIERS * sizeof(ssmp_barrier_t);
  sizeckp = SSMP_NUM_BARRIERS * num_procs * sizeof(ssmp_chk_t);
  sizeui = num_procs * sizeof(int);
  size = sizem + sizeb + sizeckp + sizeui;

  char keyF[100];
  sprintf(keyF,"/ssmp_mem");

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd<0)
    {
      if (errno != EEXIST)
	{
	  perror("In shm_open");
	  exit(1);
	}

      //this time it is ok if it already exists
      ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd<0)
	{
	  perror("In shm_open");
	  exit(1);
	}
    }
  else
    {
      if (ftruncate(ssmpfd, size) < 0) {
	perror("ftruncate failed\n");
	exit(1);
      }
    }

  ssmp_mem = (ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (ssmp_mem == NULL || (unsigned int) ssmp_mem == 0xFFFFFFFF) {
    perror("ssmp_mem = NULL\n");
    exit(134);
  }

  long long unsigned int mem_just_int = (long long unsigned int) ssmp_mem;
  ssmp_barrier = (ssmp_barrier_t *) (mem_just_int + sizem);
  ssmp_chk_t *chks = (ssmp_chk_t *) (mem_just_int + sizem + sizeb);
  ues_initialized = (int *) (mem_just_int + sizem + sizeb + sizeckp);
  int ue;
  for (ue = 0; ue < num_procs; ue++) {
    ues_initialized[ue] = 0;
  }
  
  for (ue = 0; ue < SSMP_NUM_BARRIERS * num_procs; ue++) {
    chks[ue] = 0;
  }

  int bar;
  for (bar = 0; bar < SSMP_NUM_BARRIERS; bar++) {
    ssmp_barrier[bar].checkpoints = (chks + (bar * num_procs));
    ssmp_barrier_init(bar, 0xFFFFFFFFFFFFFFFF, NULL);
  }
  ssmp_barrier_init(1, 0xFFFFFFFFFFFFFFFF, color_app);


}

void ssmp_mem_init(int id, int num_ues) {
  ssmp_recv_buf = (ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  ssmp_send_buf = (ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  if (ssmp_recv_buf == NULL || ssmp_send_buf == NULL) {
    perror("malloc@ ssmp_mem_init\n");
    exit(-1);
  }

  int core;
  for (core = 0; core < num_ues; core++) {
    ssmp_recv_buf[core] = ssmp_mem + (id * num_ues) + core;
    ssmp_recv_buf[core]->state = 0;

    ssmp_send_buf[core] = ssmp_mem + (core * num_ues) + id;
  }

  ssmp_id_ = id;
  ssmp_num_ues_ = num_ues;
  last_recv_from = (id + 1) % num_ues;

  ues_initialized[id] = 1;

  //  SP("waiting for all to be initialized!");
  int ue;
  for (ue = 0; ue < num_ues; ue++) {
    while(!ues_initialized[ue]);
  }
  //  SP("\t\t\tall initialized!");
}

void ssmp_term() {
  shm_unlink("/ssmp_mem");
}




/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */
inline void ssmp_send(int to, int w0, int w1, int w2, int w3) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  while(m->state);      

  m->w0 = w0;
  m->w1 = w1;
  m->w2 = w2;
  m->w3 = w3;

  m->state = 1;

  PD("sent to %d", to);
}

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

inline void ssmp_send7(int to, int w0, int w1, int w2, int w3, int w4, int w5, int w6) {
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  while(m->state);      

  m->w0 = w0;
  m->w1 = w1;
  m->w2 = w2;
  m->w3 = w3;
  m->w4 = w4;
  m->w5 = w5;
  m->w6 = w6;

  m->state = 1;

  PD("sent to %d", to);
}

inline int ssmp_send_try(int to, int w0, int w1, int w2, int w3) {
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
inline int ssmp_send_try7(int to, int w0, int w1, int w2, int w3, int w4, int w5, int w6) { 
  ssmp_msg_t *m = ssmp_send_buf[to];
  
  if (!m->state) {
    m->w0 = w0;
    m->w1 = w1;
    m->w2 = w2;
    m->w3 = w3;
    m->w4 = w4;
    m->w5 = w5;
    m->w6 = w6;

    m->state = 1;
    return 1;
  }
  return 0;
}


/* ------------------------------------------------------------------------------- */
/* broadcasting functions */
/* ------------------------------------------------------------------------------- */

inline void ssmp_broadcast(int w0, int w1, int w2, int w3) {
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send(core, w0, w1, w2, w3);
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

inline void ssmp_broadcast7(int w0, int w1, int w2, int w3, int w4, int w5, int w6) { 
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send7(core, w0, w1, w2, w3, w4, w5, w6);
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
    
    if (ssmp_send_try(try_to, w0, w1, w2, w3)) {
      num_to_send--;
      sent[try_to] = 1;
    }
  }

  free(sent);
}


/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void ssmp_recv_from(int from, ssmp_msg_t *msg) {
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

  msg->sender = from;
  msg->w0 = m->w0;
  msg->w1 = m->w1;
  msg->w2 = m->w2;
  msg->w3 = m->w3;
  msg->w4 = m->w4;
  msg->w5 = m->w5;


  m->state = 0;
  PD("recved from %d\n", from);
}


inline int ssmp_recv_from_try(int from, ssmp_msg_t *msg) {
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


inline void ssmp_recv(ssmp_msg_t *msg) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try(last_recv_from, msg)) {
      return;
    }
  }
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


inline int ssmp_recv_try(ssmp_msg_t *msg) {
  int i;
  for (i = 0; i < ssmp_num_ues_; i++) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try(last_recv_from, msg)) {
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

/* ------------------------------------------------------------------------------- */
/* color-based recv fucntions */
/* ------------------------------------------------------------------------------- */

inline void ssmp_color_buf_init(ssmp_color_buf_t *cbuf, int (*color)(int)) {
  if (cbuf == NULL) {
    cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
    if (cbuf == NULL) {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }
  }
  unsigned int *participants = (unsigned int *) malloc(ssmp_num_ues_ * sizeof(unsigned int));
  if (participants == NULL) {
    perror("malloc @ ssmp_color_buf_init");
    exit(-1);
  }

  int ue, num_ues = 0;
  for (ue = 0; ue < ssmp_num_ues_; ue++) {
      participants[ue] = color(ue);
      if (participants[ue]) {
	num_ues++;
      }
  }

  cbuf->num_ues = num_ues;

  cbuf->buf = (ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  if (cbuf->buf == NULL) {
    perror("malloc @ ssmp_color_buf_init");
    exit(-1);
  }

  cbuf->from = (int *) malloc(num_ues * sizeof(int));
  if (cbuf->from == NULL) {
    perror("malloc @ ssmp_color_buf_init");
    exit(-1);
  }
    
  int buf_num = 0;
  for (ue = 0; ue < ssmp_num_ues_; ue++) {
      if (participants[ue]) {
	cbuf->buf[buf_num] = ssmp_recv_buf[ue];
	cbuf->from[buf_num] = ue;
	buf_num++;
      }
  }

  free(participants);
}

inline void ssmp_color_buf_free(ssmp_color_buf_t *cbuf) {
  free(cbuf->buf);
}

inline void ssmp_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg) {
  int from;
  
  while(1) {
    //XXX: maybe have a last_recv_from field
    for (from = 0; from < cbuf->num_ues; from++) {
      ssmp_msg_t *m = cbuf->buf[from];
      if (m->state) {

	msg->sender = cbuf->from[from];
	msg->w0 = m->w0;
	msg->w1 = m->w1;
	msg->w2 = m->w2;
	msg->w3 = m->w3;

	m->state = 0;
	return;
      }
    }
  }
}

/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */

int color_app(int id) {
  return ((id % 2) ? 1 : 0);
}

inline ssmp_barrier_t * ssmp_get_barrier(int barrier_num) {
  if (barrier_num < SSMP_NUM_BARRIERS) {
    return (ssmp_barrier + barrier_num);
  }
  else {
    return NULL;
  }
}

int color(int id) {
  return (10*id);
}

inline void ssmp_barrier_init(int barrier_num, long long int participants, int (*color)(int)) {
  if (barrier_num >= SSMP_NUM_BARRIERS) {
    return;
  }

  ssmp_barrier[barrier_num].participants = 0xFFFFFFFFFFFFFFFF;
  ssmp_barrier[barrier_num].color = color;
  int ue;
  for (ue = 0; ue < ssmp_num_ues_; ue++) {
    ssmp_barrier[barrier_num].checkpoints[ue] = 0;
  }
  ssmp_barrier[barrier_num].version = 0;
}

inline void ssmp_barrier_wait(int barrier_num) {
  if (barrier_num >= SSMP_NUM_BARRIERS) {
    return;
  }

  ssmp_barrier_t *b = &ssmp_barrier[barrier_num];
  unsigned int version = b->version;

  SP(">>Waiting barrier %d\t(v: %d)", barrier_num, version);

  int (*col)(int);
  col= b->color;

  unsigned int *participants = (unsigned int *) malloc(ssmp_num_ues_ * sizeof(unsigned int));
  if (participants == NULL) {
    perror("malloc @ ssmp_barrier_wait");
    exit(-1);
  }
  long long unsigned int bpar = b->participants;
  int from;
  for (from = 0; from < ssmp_num_ues_; from++) {
    /* if there is a color function it has priority */
    if (col != NULL) {
      participants[from] = col(from);
    }
    else {
      participants[from] = (unsigned int) (bpar & 0x0000000000000001);
      bpar >>= 1;
    }
  }
  
  if (participants[ssmp_id_] == 0) {
    SP("<<Cleared barrier %d\t(v: %d)\t[not participant!]", barrier_num, version);
    free(participants);
    return;
  }

  //round 1;
  b->checkpoints[ssmp_id_] = version + 1;
  
  int done = 0;
  while(!done) {
    done = 1;
    int ue;
    for (ue = 0; ue < ssmp_num_ues_; ue++) {
      if (participants[ue] == 0) {
	continue;
      }
      
      if ((b->checkpoints[ue] != (version + 1)) && (b->checkpoints[ue] != (version + 2))) {
	done = 0;
	break;
      }
    }
  }

  //round 2;
  b->checkpoints[ssmp_id_] = version + 2;

  done = 0;
  while(!done) {
    done = 1;
    int ue;
    for (ue = 0; ue < ssmp_num_ues_; ue++) {
      if (participants[ue] == 0) {
	continue;
      }
      
      if (b->version > version) {
	SP("<<Cleared barrier %d\t(v: %d)\t[someone was faster]", barrier_num, version);
	b->checkpoints[ssmp_id_] = 0;
	return;
      }

      if (b->checkpoints[ue] == (version + 1)) {
	done = 0;
	break;
      }
    }
  }

  b->checkpoints[ssmp_id_] = 0;
  if (b->version <= version) {
    b->version = version + 3;
  }

  free(participants);
  SP("<<Cleared barrier %d (v: %d)", barrier_num, version);
}


/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */

inline double wtime(void)
{
  struct timeval t;
  gettimeofday(&t,NULL);
  return (double)t.tv_sec + ((double)t.tv_usec)/1000000.0;
}

void set_cpu(int cpu) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
    printf("Problem with setting processor affinity: %s\n",
	   strerror(errno));
    exit(3);
  }
}

#if defined(__i386__)
inline ticks getticks(void) {
  ticks ret;

  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}
#elif defined(__x86_64__)
inline ticks getticks(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif

inline int ssmp_id() {
  return ssmp_id_;
}

inline int ssmp_num_ues() {
  return ssmp_num_ues_;
}
