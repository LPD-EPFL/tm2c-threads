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
  printf("NUM of msgs: %lld\n", nm);
  printf("using colored buf recv!\n");

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

  ssmp_barrier_wait(0);
  P("CLEARED barrier %d", 0);

  double _start = wtime();
  ticks _start_ticks = getticks();

  ssmp_msg_t msg;

  if (ID % 2 == 0) {
    P("service core!");
    
    ssmp_color_buf_t cbuf;
    ssmp_color_buf_init(&cbuf, color_app);

    unsigned int from[48];
    from[0] = from[1] = from[2] = from[3] = 0;
    while(1) {
      //      ssmp_recv6(&msg);
      ssmp_recv_color(&cbuf, &msg);
      //      P("reved from %d", msg.sender);
      if (msg.w0 < 0) {
	P("exiting ..");
	int co;
	for (co = 0; co < ssmp_num_ues(); co++) {
	  P("from[%d] = %d", co, from[co]);
	}
	exit(0);
      }
      from[msg.sender]++;
      ssmp_send2(msg.sender, msg.w1, msg.w3);
    }
  }
  else {
    P("app core!");
    int to = 0;
    long long int nm1 = nm;
    
    while (nm1--) {
      to = (to + 2) % num_procs;
      ssmp_send6(to, nm1, nm1, nm1, nm1, nm1, to);
      ssmp_recv_from(to, &msg);
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
