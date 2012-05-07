#include "ssmp.h"

//#define SSMP_DEBUG


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
/* color-based initialization fucntions */
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

  PD(">>Waiting barrier %d\t(v: %d)", barrier_num, version);

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
    PD("<<Cleared barrier %d\t(v: %d)\t[not participant!]", barrier_num, version);
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
	PD("<<Cleared barrier %d\t(v: %d)\t[someone was faster]", barrier_num, version);
	b->checkpoints[ssmp_id_] = 0;
	free(participants);
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
  PD("<<Cleared barrier %d (v: %d)", barrier_num, version);
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
