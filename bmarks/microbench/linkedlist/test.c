/*
 * Adapted to TM2C by Vasileios Trigonakis <vasileios.trigonakis@epfl.ch> 
 *
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses of the linked list
 *
 * Copyright (c) 2009-2010.
 *
 * test.c is part of Microbench
 * 
 * Microbench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "linkedlist.h"
#include <unistd.h>

#ifdef SEQUENTIAL
#ifdef BARRIER
#undef BARRIER
#define BARRIER BARRIERW
#endif
#endif

int argc;
char **argv;
intset_t* set;

/* ################################################################### *
 * RANDOM
 * ################################################################### */

/* Re-entrant version of rand_range(r) */
#define rand_range_re(dummy, r) (tm2c_rand() % r)

typedef struct thread_data 
{
  val_t first;
  long range;
  int update;
  int unit_tx;
  int alternate;
  int effective;
  unsigned long nb_add;
  unsigned long nb_added;
  unsigned long nb_remove;
  unsigned long nb_removed;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned int seed;
  intset_t* set;
  unsigned long failures_because_contention;
} thread_data_t;

volatile int work = 1;

void
alarm_handler(int sig)
{
  work = 0;
}

void*
test(void* data, double duration) 
{
  int unext, last = -1;
  val_t val = 0;

  thread_data_t* d = (thread_data_t*) data;

  srand_core();

  /* Create transaction */

  /* Is the first op an update? */
  unext = (rand_range(100) < d->update);

  ONCE {
	  signal (SIGALRM, alarm_handler);
	  alarm(duration);
  }

  BARRIER;
  while(work)
    {
      if (unext) { // update

	if (last < 0) { // add

	  val = rand_range_re(&d->seed, d->range);
	  if (set_add(d->set, val, TRANSACTIONAL)) {
	    d->nb_added++;
	    last = val;
	  }
	  d->nb_add++;

	}
	else { // remove

	  if (d->alternate) { // alternate mode (default)
	    if (set_remove(d->set, last, TRANSACTIONAL)) {
	      d->nb_removed++;
	    }
	    last = -1;
	  }
	  else {
	    /* Random computation only in non-alternated cases */
	    val = rand_range_re(&d->seed, d->range);
	    /* Remove one random value */
	    if (set_remove(d->set, val, TRANSACTIONAL)) {
	      d->nb_removed++;
	      /* Repeat until successful, to avoid size variations */
	      last = -1;
	    }
	  }
	  d->nb_remove++;
	}
      }
      else { // read

	if (d->alternate) {
	  if (d->update == 0) {
	    if (last < 0) {
	      val = d->first;
	      last = val;
	    }
	    else { // last >= 0
	      val = rand_range_re(&d->seed, d->range);
	      last = -1;
	    }
	  }
	  else { // update != 0
	    if (last < 0) {
	      val = rand_range_re(&d->seed, d->range);
	      //last = val;
	    }
	    else {
	      val = last;
	    }
	  }
	}
	else val = rand_range_re(&d->seed, d->range);

	if (set_contains(d->set, val, TRANSACTIONAL))
	  d->nb_found++;
	d->nb_contains++;

      }

      /* Is the next op an update? */
      if (d->effective) { // a failed remove/add is a read-only tx
	unext = ((100 * (d->nb_added + d->nb_removed))
		 < (d->update * (d->nb_add + d->nb_remove + d->nb_contains)));
      }
      else { // remove/add (even failed) is considered as an update
	unext = (rand_range_re(&d->seed, 100) < d->update);
      }
    } 

  duration__ = duration;

  return NULL;
}

void *mainthread(void *args) {

#ifndef SEQUENTIAL
  TM_START;
#else
  //SEQ_INIT;
#endif

  struct option long_options[] =
    {
      // These options don't set a flag
      {"help", no_argument, NULL, 'h'},
      {"verbose", no_argument, NULL, 'v'},
      {"duration", required_argument, NULL, 'd'},
      {"initial-size", required_argument, NULL, 'i'},
      {"range", required_argument, NULL, 'r'},
      {"update-rate", required_argument, NULL, 'u'},
      {"elasticity", required_argument, NULL, 'x'},
      {"effective", required_argument, NULL, 'f'},
      {NULL, 0, NULL, 0}
    };

  int i, c;
  static int size = 0;
  static val_t last = 0;
  static val_t val = 0;
  static double duration = DEFAULT_DURATION;
  static int initial = DEFAULT_INITIAL;
  static int nb_app_cores;
#if defined(SEQUENTIAL)
  static nb_app_cores = 1;
#endif
  static long range = DEFAULT_RANGE;
  static int update = DEFAULT_UPDATE;
  static int unit_tx = DEFAULT_ELASTICITY;
  static int alternate = DEFAULT_ALTERNATE;
  static int effective = DEFAULT_EFFECTIVE;
  static int verbose = DEFAULT_VERBOSE;
  unsigned int seed = 0;

  ONCE {
	  nb_app_cores = NUM_APP_NODES;
	 while (1)
		{
		  i = 0;
		  c = getopt_long(argc, argv, "hAf:d:i:r:u:x:v", long_options, &i);

		  if (c == -1)
		break;

		  if (c == 0 && long_options[i].flag == 0)
		c = long_options[i].val;

		  switch (c) {
		  case 0:
		/* Flag is automatically set */
		break;
		  case 'h':
		ONCE
		  {
			printf("intset -- STM stress test "
			   "(linked list)\n"
			   "\n"
			   "Usage:\n"
			   "  intset [options...]\n"
			   "\n"
			   "Options:\n"
			   "  -h, --help\n"
			   "        Print this message\n"
			   "  -A, --alternate (default="XSTR(DEFAULT_ALTERNATE)")\n"
			   "        Consecutive insert/remove target the same value\n"
			   "  -f, --effective <int>\n"
			   "        update txs must effectively write (0=trial, 1=effective, default=" XSTR(DEFAULT_EFFECTIVE) ")\n"
			   "  -d, --duration secs<double>\n"
			   "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
			   "  -i, --initial-size <int>\n"
			   "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
			   "  -r, --range <int>\n"
			   "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
			   "  -u, --update-rate <int>\n"
			   "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
			   "  -v , --verbose\n"
			   "        Print detailed stats"
			   );
		  }
		exit(1);
		  case 'A':
		alternate = 1;
		break;
		  case 'f':
		effective = atoi(optarg);
		break;
		  case 'd':
		duration = atof(optarg);
		break;
		  case 'i':
		initial = atoi(optarg);
		break;
		  case 'r':
		range = atol(optarg);
		break;
		  case 'u':
		update = atoi(optarg);
		break;
		  case 'x':
		unit_tx = atoi(optarg);
		break;
		  case 'v':
		verbose = 1;
		break;
		  case '?':
		ONCE
		  {
			printf("Use -h or --help for help\n");
		  }
		  default:
			  exit(1);
		  }
		}

  }
	 if (seed == 0)
		{
		  srand_core();
		  seed = rand_range((NODE_ID() + 17) * 123);
		  srand(seed);
		}
	  else
		srand(seed);


	  ONCE
		{
		  assert(duration >= 0);
		  assert(initial >= 0);
		  assert(nb_app_cores > 0);
		  assert(range > 0 && range >= initial);
		  assert(update >= 0 && update <= 100);

		  printf("Bench type   : linked list\n");
	#ifdef SEQUENTIAL
		  printf("                sequential\n");
	#elif defined(EARLY_RELEASE )
		  printf("                using early-release\n");
	#elif defined(READ_VALIDATION)
		  printf("                using read-validation\n");
	#endif
	#ifdef LOCKS
		  printf("                  with locks\n");
	#endif
		  printf("Duration     : %f\n", duration);
		  printf("Initial size : %d\n", initial);
		  printf("Nb cores     : %d\n", nb_app_cores);
		  printf("Value range  : %ld\n", range);
		  printf("Update rate  : %d\n", update);
		  printf("Elasticity   : %d\n", unit_tx);
		  printf("Alternate    : %d\n", alternate);
		  printf("Effective    : %d\n", effective);
		  FLUSH;
		}


  thread_data_t* data;
  if ((data = (thread_data_t*) malloc(sizeof (thread_data_t))) == NULL)
    {
      perror("malloc");
      exit(1);
    }

  shmem_init(10 * 1024 * NODE_ID() * sizeof (node_t) + ((initial + 2) * sizeof (node_t)));
  ONCE {
	  set = set_new();
  }
  _mm_mfence();
  BARRIER;

  ONCE
    {
      /* Populate set */
      /* printf("Adding %d entries to set\n", initial); */
      i = 0;
      while (i < initial) {
	val = rand_range(range);
	if (set_add(set, val, 0)) {
	  last = val;
	  i++;
	}
      }
      size = set_size(set);
      /* set_print(set); */
      printf("Set size     : %d\n", size);
      assert(size == initial);
      FLUSH
	}

  BARRIER;
  /* Access set from all threads */
  data->first = last;
  data->range = range;
  data->update = update;
  data->unit_tx = unit_tx;
  data->alternate = alternate;
  data->effective = effective;
  data->nb_add = 0;
  data->nb_added = 0;
  data->nb_remove = 0;
  data->nb_removed = 0;
  data->nb_contains = 0;
  data->nb_found = 0;
  data->set = set;
  data->seed = seed;

  BARRIER;
  /* Start */
  test(data, duration);

  if (verbose)
    {
      APP_EXEC_ORDER
	{
	  printf("-- Core %d\n", NODE_ID());
	  printf("  #add        : %lu\n", data->nb_add);
	  printf("    #added    : %lu\n", data->nb_added);
	  printf("  #remove     : %lu\n", data->nb_remove);
	  printf("    #removed  : %lu\n", data->nb_removed);
	  printf("  #contains   : %lu\n", data->nb_contains);
	  printf("    #found    : %lu\n", data->nb_found);
	  printf("---------------------------------------------------");
	  FLUSH;
	} APP_EXEC_ORDER_END;
    }
  /* Delete set */

  BARRIER;

  ONCE
    {
      int size_after = set_size(set);
      /* set_print(set); */
      printf("Set size (af): %u\n", size_after);
      node_t *head = set->headp;
      node_t *node = head->nextp;
      while (node->nextp != NULL) {
         if (node->val > node->nextp->val) {
            fprintf(stderr, "Error on list integrity\n");
         }
			node = node->nextp;
      }
    }

  BARRIER;
  
#ifdef SEQUENTIAL
  int total_ops = data->nb_add + data->nb_contains + data->nb_remove;
  printf("#Ops          : %d\n", total_ops);
  printf("#Ops/s        : %d\n", (int) (total_ops / duration__));
  printf("#Latency      : %f\n", duration__ / total_ops);
  FLUSH;
#endif

  //set_delete(set);

  /* Cleanup STM */

  free(data);
  BARRIER;

#ifndef SEQUENTIAL
  TM_END;
#endif

  EXIT(0);
}


int main(int argc2, char** argv2) {

	argc = argc2;
	argv = argv2;
	TM2C_INIT_SYS;
	TM2C_INIT_THREAD;
	EXIT(0);
}
