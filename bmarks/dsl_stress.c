/*
 * File:
 *   dsl_stress.c
 * Author(s): Vasileios Trigonakis
 */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>

#include "tm.h"

#define DEFAULT_NUM_OPS                 1000000
#define DEFAULT_DELAY                   0
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1

int delay = DEFAULT_DELAY;


#define XSTR(s)                         STR(s)
#define STR(s)                          #s

uint32_t* data;


static uint8_t
closest_dsl(uint8_t id)
{
  int8_t c;
  for (c = id - 1; c >= 0; c--)
    {
      if (is_dsl_core(c))
	{
	  return c;
	}
    }
}



/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

uint32_t
test(uint32_t num_ops, int nb_accounts) 
{
  uint8_t to = closest_dsl(NODE_ID());
  uint8_t to_seq = dsl_id_seq(to);
  PRINT("sending to %02d (seq id %02d)", to, to_seq);

  BARRIER;

  uint32_t tot = 0;

  /* double __ticks_per_sec = 1000000000*REF_SPEED_GHZ;	 */
  /* ticks __duration_ticks; */
  /* ticks __end_ticks; */
  /* ticks __start_ticks = getticks();			 */
  FOR(2)
    {
      /* uint32_t op; */
      /* for (op = 0; op < num_ops_per_rep; op++) */
      /*   { */
      ps_dummy_msg(to_seq);
      tot++;
      /* } */
    } END_FOR;

  /* to++; */
  /* if (to == NUM_DSL_NODES) */
  /*   { */
  /* 	to = 0; */
  /*   } */
/* }  */
  /* __end_ticks = getticks();			 */
  /* __duration_ticks = __end_ticks - __start_ticks; */
  /* duration__ = __duration_ticks / __ticks_per_sec; */


  /* BARRIER; */
  PRINT("done in %f  -- %u", duration__, tot);
  BARRIER;

  return tot;
}

TASKMAIN(int argc, char **argv) {
  dup2(STDOUT_FILENO, STDERR_FILENO);

  TM_INIT;

  struct option long_options[] = {
    // These options don't set a flag
    {"help", no_argument, NULL, 'h'},
    {"accounts", required_argument, NULL, 'a'},
    {"contention-manager", required_argument, NULL, 'c'},
    {"num_ops", required_argument, NULL, 'd'},
    {"delay", required_argument, NULL, 'D'},
    {"read-all-rate", required_argument, NULL, 'r'},
    {"check", required_argument, NULL, 'c'},
    {"read-threads", required_argument, NULL, 'R'},
    {"write-all-rate", required_argument, NULL, 'w'},
    {"write-threads", required_argument, NULL, 'W'},
    {"disjoint", no_argument, NULL, 'j'},
    {NULL, 0, NULL, 0}
  };

  int i, c;
  int num_ops = DEFAULT_NUM_OPS;
  int nb_accounts = DEFAULT_NB_ACCOUNTS;
  int nb_app_cores = NUM_APP_NODES;

  while (1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "h:a:d:l", long_options, &i);

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
	    PRINT("dsl_stress -- STM server stress test\n"
		  "\n"
		  "Usage:\n"
		  "  dsl_stress [options...]\n"
		  "\n"
		  "Options:\n"
		  "  -h, --help\n"
		  "        Print this message\n"
		  "  -a, --accounts <int>\n"
		  "        Number of accounts in the bank (default=" XSTR(DEFAULT_NB_ACCOUNTS) ")\n"
		  "  -d, --num_ops <double>\n"
		  "        Test num_ops in seconds (0=infinite, default=" XSTR(DEFAULT_NUM_OPS) ")\n"
		  "  -l, --delay <int>\n"
		  "        Delay ns after succesfull request. Used to understress the system, default=" XSTR(DEFAULT_DELAY) ")\n"
		  );
	  }
	exit(0);
      case 'a':
	nb_accounts = atoi(optarg);
	break;
      case 'd':
	num_ops = atoi(optarg);
	break;
      case 'D':
	delay = atoi(optarg);
	break;
      case '?':
	ONCE
	  {
	    PRINT("Use -h or --help for help\n");
	  }

	exit(0);
      default:
	exit(1);
      }
    }

  assert(num_ops >= 0);
  assert(nb_accounts >= 2);
  assert(nb_app_cores > 0);
  ONCE
    {
      PRINTN("Nb accounts    : %d\n", nb_accounts);
      PRINTN("Num Ops        : %d\n", num_ops);
      PRINTN("Nb cores       : %d\n", nb_app_cores);
    }


  BARRIER;

  num_ops = test(num_ops, nb_accounts);

  /* End it */

  /* BARRIER; */

  stm_tx_node->tx_commited = num_ops;

  TM_END;

  EXIT(0);
}

