#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>

#include "common.h"
#include "ssmp.h"

#define NUM_ELEMS  1024

int num_procs = 2;
long long int nm = 100000000;
int ID;

int main(int argc, char **argv) {
  if (argc > 1) {
    num_procs = atoi(argv[1]);
  }
  if (argc > 2) {
    nm = atol(argv[2]);
  }

  ID = 0;
  printf("NUM of processes: %d\n", num_procs);
  printf("NUM of msgs: %lld (%d bytes each)\n", nm, sizeof(int) * NUM_ELEMS);

  ssmp_init(num_procs);

  int rank;
  for (rank = 1; rank < num_procs; rank++) {
    P("Forking child %d", rank);
    pid_t child = fork();
    if (child < 0) {
      P("Failure in fork():\n%s", strerror(errno));
    } else if (child == 0) {
      goto fork_done;
    }
  }
  rank = 0;

 fork_done:
  ID = rank;
  P("Initializing child %u", rank);
  set_cpu(ID);
  ssmp_mem_init(ID, num_procs);
  P("Initialized child %u", rank);

  ssmp_color_buf_t cbuf;
  ssmp_color_buf_init(&cbuf, color_app);

  int data[NUM_ELEMS]; 
  int l;
  for (l = 0; l < NUM_ELEMS; l++) {
    data[l] = ID;
  }

  ssmp_barrier_wait(0);
  P("CLEARED barrier %d", 0);

  double _start = wtime();
  ticks _start_ticks = getticks();

  ssmp_msg_t msg;

  if (ID % 2 == 0) {
    P("service core!");

    unsigned int from[48];
    while(1) {
      ssmp_recv_color(&cbuf, &msg, 24);
      if (msg.w0 < 0) {
	P("exiting ..");
	exit(0);
      }
      ssmp_recv_from_big(msg.sender, data, msg.w0);
      int l, sum = 0;
      for (l = 0; l < msg.w2; l++) {
	sum += data[l];
      }
      if (sum != msg.w1) {
	P("expected sum %d, got %d", msg.w1, sum);
      }
    }
  }
  else {
    P("app core!");
    int to = ssmp_id() - 1;
    long long int nm1 = nm;
    
    msg.w0 = sizeof(int) * NUM_ELEMS;
    msg.w1 = ssmp_id() * NUM_ELEMS;
    msg.w2 = NUM_ELEMS;

    while (nm1--) {
      to = (to + 2) % num_procs;
      ssmp_send(to, &msg, 12);
      ssmp_send_big(to, data, msg.w0);
    }
  }

  

  ticks _end_ticks = getticks();
  double _end = wtime();

  double _time = _end - _start;
  ticks _ticks = _end_ticks - _start_ticks;
  ticks _ticksm =(ticks) ((double)_ticks / nm);
  double lat = (double) (1000*1000*1000*_time) / nm;
  printf("sent %lld msgs\n\t"
	 "in %f secs\n\t"
	 "%.2f msgs/us\n\t"
	 "%f ns latency\n"
	 "in ticks:\n\t"
	 "in %lld ticks\n\t"
	 "%lld ticks/msg\n", nm, _time, ((double)nm/(1000*1000*_time)), lat,
	 _ticks, _ticksm);


  ssmp_barrier_wait(1);
  if (ssmp_id() == 1) {
    P("terminating --");
    int core; 
    for (core = 0; core < ssmp_num_ues(); core++) {
      if (core % 2 == 0) {
	ssmp_send1(core, -1);
      }
    }
  }

  ssmp_term();
  return 0;
}
