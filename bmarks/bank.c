/*
 * File:
 *   bank.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Bank stress test.
 *
 * Copyright (c) 2007-2011.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>

#include "tm.h"

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */

/*use TX_LOAD_STORE*/
#define LOAD_STORE


#if defined(SCC)
/*take advante all 4 MCs*/
#define MC__
#endif

#define DEFAULT_DURATION                10
#define DEFAULT_DELAY                   0
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                0
#define DEFAULT_CHECK                   0
#define DEFAULT_WRITE_ALL               0
#define DEFAULT_READ_THREADS            0
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0

int delay = DEFAULT_DELAY;


#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define CAST_VOIDP(addr)                ((void *) (addr))

#define MB16            16777216
#define INDEX(i)        ((((i) % 4) * MB16) + (i))

#ifdef MC
#define I(i)            INDEX(i)
#else
#define I(i)            i
#endif


/* ################################################################### *
 * GLOBALS
 * ################################################################### */

/* ################################################################### *
 * BANK ACCOUNTS
 * ################################################################### */

typedef struct account 
{
  int32_t number;
  int32_t balance;
} account_t;

typedef struct bank 
{
  account_t *accounts;
  uint32_t size;
} bank_t;

int 
transfer(account_t *src, account_t *dst, int amount) 
{
  /* PRINT("in transfer"); */

  int i, j;


  /* Allow overdrafts */
  /* PF_START(2); */
  TX_START;
  /* PF_STOP(2); */

#ifdef LOAD_STORE
  /* PF_START(0); */
  TX_LOAD_STORE(&src->balance, -, amount, TYPE_INT);
  /* PF_STOP(0); */
  /* PF_START(1); */
  TX_LOAD_STORE(&dst->balance, +, amount, TYPE_INT);
  /* PF_STOP(1); */
    
  /* PF_START(3); */
  TX_COMMIT_NO_PUB;
  /* PF_STOP(3); */
#else
  i = *(int *) TX_LOAD(&src->balance);
  i -= amount;
  TX_STORE(&src->balance, &i, TYPE_INT); //NEED TX_STOREI
  j = *(int *) TX_LOAD(&dst->balance);
  j += amount;
  TX_STORE(&dst->balance, &j, TYPE_INT);
  TX_COMMIT;
#endif

  return amount;
}

void
check_accs(account_t *acc1, account_t *acc2) 
{
  /* PRINT("in transfer"); */

  int i, j;
  
  TX_START;
  i = *(int *) TX_LOAD(&acc1->balance);
  j = *(int *) TX_LOAD(&acc2->balance);
  TX_COMMIT;

  if (i + j == 123000)
    {
      PRINT("just disallowing the compiler to do dead-code elimination");
    }
}


int total(bank_t *bank, int transactional) {
  int i, total;

  if (!transactional) {
    total = 0;
    for (i = 0; i < bank->size; i++) {
#ifndef PLATFORM_CLUSTER
      total += bank->accounts[I(i)].balance;
#else
      total += NONTX_LOAD(&bank->accounts[i].balance);
#endif

    }
  }
  else {
    /* PF_START(3); */
    TX_START;
    total = 0;
    for (i = 0; i < bank->size; i++)
      {
#ifndef PGAS
	/* PF_START(4); */
	int *bal = (int*) TX_LOAD(&bank->accounts[I(i)].balance); 
	/* PF_STOP(4) */
	total += *bal;
#else
	total += TX_LOAD(&bank->accounts[i].balance);
#endif
      }
    TX_COMMIT;
    /* PF_STOP(3); */
  }

  if (total != 0) {
    PRINT("Got a bank total of: %d", total);
  }

  return total;
}

void reset(bank_t *bank) {
    int i, j = 0;

    TX_START
    for (i = 0; i < bank->size; i++) {
#ifdef PGAS
        TX_STORE(&bank->accounts[i].balance, j, TYPE_INT);
#else
        TX_STORE(&bank->accounts[I(i)].balance, j, TYPE_INT);
#endif
    }
    TX_COMMIT
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data {
  uint32_t nb_transfer;
  uint32_t nb_checks;
  uint32_t nb_read_all;
  uint32_t nb_write_all;
  uint32_t id;
  uint32_t read_cores;
  uint32_t write_cores;
  uint32_t read_all;
  uint32_t write_all;
  uint32_t check;
  uint32_t disjoint;
  uint32_t nb_app_cores;
  uint32_t padding[4];
} thread_data_t;

bank_t * 
test(void *data, double duration, int nb_accounts) 
{
  int rand_max, rand_min;
  thread_data_t *d = (thread_data_t *) data;
  bank_t * bank;


  /* Initialize seed (use rand48 as rand is poor) */
  srand_core();
  BARRIER;

  /* Prepare for disjoint access */
  if (d->disjoint) 
    {
      PRINT("disjoint not supported by the way rand_max is calculated");
      rand_max = nb_accounts / d->nb_app_cores;
      rand_min = rand_max * d->id;
      if (rand_max <= 2) {
	fprintf(stderr, "can't have disjoint account accesses");
	return NULL;
      }
    }
  else 
    {
      rand_max = nb_accounts - 1;
      rand_min = 0;
    }

  bank = (bank_t *) malloc(sizeof (bank_t));
  if (bank == NULL) 
    {
      PRINT("malloc bank");
      EXIT(1);
    }

#ifdef MC
  bank->accounts = (account_t *) sys_shmalloc(64 * 1024 * 1024);
#else
  bank->accounts = (account_t *) sys_shmalloc(nb_accounts * sizeof (account_t));
#endif

  if (bank->accounts == NULL) {
    PRINT("malloc bank->accounts");
    EXIT(1);
  }

  bank->size = nb_accounts;

  ONCE
    {
      int i;
      for (i = 0; i < bank->size; i++) {
	//       PRINTN("(s %d)", i);
	NONTX_STORE(&bank->accounts[i].number, i, TYPE_INT);
	NONTX_STORE(&bank->accounts[i].balance, 0, TYPE_INT);
      }
    }

  ONCE
    {
      PRINT("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\tBank total (before): %d\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
	    total(bank, 0));
    }

  /* Wait on barrier */
  BARRIER;

  /* warming up */
  TX_START;
  TX_COMMIT_NO_STATS;
  total(bank, 0);

  uint32_t trans = 0;
  BARRIER;

  FOR(duration)
  /* FOR_ITERS(1000000) */
  {
    /* if (d->read_cores && d->id < d->read_cores)  */
    /*   { */
    /* 	/\* Read all *\/ */
    /* 	//  PRINT("READ ALL1"); */
    /* 	total(bank, 1); */
    /* 	d->nb_read_all++; */
    /* 	continue; */
    /*   } */

    uint8_t nb = tm2c_rand() & 127;
    /* if (nb < d->read_all) */
    /*   { */
    /* 	/\* Read all *\/ */
    /* 	total(bank, 1); */
    /* 	d->nb_read_all++; */
    /*   } */
    /* else */
      {
	/* Choose random accounts */
	PF_START(3);
	uint32_t src = tm2c_rand() & rand_max;
	PF_STOP(3);
	PF_START(4);
	uint32_t dst = tm2c_rand() & rand_max;
	PF_STOP(4);

	if (dst == src)
	  {
	    dst = ((src + 1) & rand_max);
	  }
	if (nb < d->check) 
	  {
	    check_accs(bank->accounts + I(src), bank->accounts + I(dst));
	    d->nb_checks++;
	  }
	else 
	  {
	    transfer(bank->accounts + I(src), bank->accounts + I(dst), 1);
	    d->nb_transfer++;
	  }
      }


    /* if (delay) */
    /*   { */
    /* 	/\* ndelay(rand_range(delay)); *\/ */
    /* 	ndelay(delay); */
    /*   } */
  } /* END_FOR_ITERS; */
  END_FOR;

  BARRIER;

  return bank;
}

TASKMAIN(int argc, char **argv) {
  dup2(STDOUT_FILENO, STDERR_FILENO);

  PF_MSG(0, "1st TX_LOAD_STORE (transfer)");
  PF_MSG(1, "2nd TX_LOAD_STORE (transfer)");
  PF_MSG(2, "the whole transfer");

  TM_INIT;

  struct option long_options[] = {
    // These options don't set a flag
    {"help", no_argument, NULL, 'h'},
    {"accounts", required_argument, NULL, 'a'},
    {"contention-manager", required_argument, NULL, 'c'},
    {"duration", required_argument, NULL, 'd'},
    {"delay", required_argument, NULL, 'D'},
    {"read-all-rate", required_argument, NULL, 'r'},
    {"check", required_argument, NULL, 'c'},
    {"read-threads", required_argument, NULL, 'R'},
    {"write-all-rate", required_argument, NULL, 'w'},
    {"write-threads", required_argument, NULL, 'W'},
    {"disjoint", no_argument, NULL, 'j'},
    {NULL, 0, NULL, 0}
  };

  bank_t *bank;
  int i, c;
  thread_data_t *data;

  double duration = DEFAULT_DURATION;
  int nb_accounts = DEFAULT_NB_ACCOUNTS;
  int nb_app_cores = NUM_APP_NODES;
  int read_all = DEFAULT_READ_ALL;
  int read_cores = DEFAULT_READ_THREADS;
  int write_all = DEFAULT_READ_ALL + DEFAULT_WRITE_ALL;
  int check = write_all + DEFAULT_CHECK;
  int write_cores = DEFAULT_WRITE_THREADS;
  int disjoint = DEFAULT_DISJOINT;


  while (1) {
    i = 0;
    c = getopt_long(argc, argv, "h:a:d:D:r:c:R:w:W:j", long_options, &i);

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
	  PRINT("bank -- STM stress test\n"
		"\n"
		"Usage:\n"
		"  bank [options...]\n"
		"\n"
		"Options:\n"
		"  -h, --help\n"
		"        Print this message\n"
		"  -a, --accounts <int>\n"
		"        Number of accounts in the bank (default=" XSTR(DEFAULT_NB_ACCOUNTS) ")\n"
		"  -d, --duration <double>\n"
		"        Test duration in seconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
		"  -d, --delay <int>\n"
		"        Delay ns after succesfull request. Used to understress the system, default=" XSTR(DEFAULT_DELAY) ")\n"
		"  -c, --check <int>\n"
		"        Percentage of check transactions transactions (default=" XSTR(DEFAULT_CHECK) ")\n"
		"  -r, --read-all-rate <int>\n"
		"        Percentage of read-all transactions (default=" XSTR(DEFAULT_READ_ALL) ")\n"
		"  -R, --read-threads <int>\n"
		"        Number of threads issuing only read-all transactions (default=" XSTR(DEFAULT_READ_THREADS) ")\n"
		"  -w, --write-all-rate <int>\n"
		"        Percentage of write-all transactions (default=" XSTR(DEFAULT_WRITE_ALL) ")\n"
		"  -W, --write-threads <int>\n"
		"        Number of threads issuing only write-all transactions (default=" XSTR(DEFAULT_WRITE_THREADS) ")\n"
		);
	}
      exit(0);
    case 'a':
      nb_accounts = atoi(optarg);
      break;
    case 'd':
      duration = atof(optarg);
      break;
    case 'D':
      delay = atoi(optarg);
      break;
    case 'c':
      check = atoi(optarg);
      break;
    case 'r':
      read_all = atoi(optarg);
      break;
    case 'R':
      read_cores = atoi(optarg);
      break;
    case 'w':
      write_all = atoi(optarg);
      PRINT("*** warning: write all has been disabled");
      break;
    case 'W':
      write_cores = atoi(optarg);
      PRINT("*** warning: write all cores have been disabled");
      break;
    case 'j':
      disjoint = 1;
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


  nb_accounts = pow2roundup(nb_accounts);

  write_all = 0;
  write_cores = 0;

  write_all += read_all;
  check += write_all;

  assert(duration >= 0);
  assert(nb_accounts >= 2);
  assert(nb_app_cores > 0);
  assert(read_all >= 0 && write_all >= 0 && check >= 0 && check <= 100);
  assert(read_cores + write_cores <= nb_app_cores);

  ONCE
    {
      PRINTN("Nb accounts    : %d\n", nb_accounts);
      PRINTN("Duration       : %fs\n", duration);
      PRINTN("Nb cores       : %d\n", nb_app_cores);
      PRINTN("Check acc rate : %d\n", check - write_all);
      PRINTN("Transfer rate  : %d\n", 100 - check);
      PRINTN("Read-all rate  : %d\n", read_all);
      PRINTN("Read cores     : %d\n", read_cores);
      PRINTN("Write-all rate : %d\n", write_all - read_all);
      PRINTN("Write cores    : %d\n", write_cores);
      PRINT("sizeof(size_t) = %d", sizeof(size_t));
      PRINT("sizeof(uintptr_t) = %d", sizeof(uintptr_t));
      PRINT("sizeof(PS_COMMAND) = %d", sizeof(PS_COMMAND));
      PRINT("sizeof(PS_STATS_CMD_T) = %d", sizeof(PS_STATS_CMD_T));
      PRINT("sizeof(PS_REPLY) = %d", sizeof(PS_REPLY));
      PRINT("sizeof(stm_tx_t) = %d", sizeof(stm_tx_t));
      PRINT("sizeof(stm_tx_node_t) = %d", sizeof(stm_tx_node_t));
      PRINT("sizeof(sigjmp_buf) = %d", sizeof(sigjmp_buf));
    }


  /* normalize percentages to 128 */

  double normalize = (double) 128/100;
  check *= normalize;
  write_all *= normalize;
  read_all *= normalize;


  if (posix_memalign((void**) &data, 64, sizeof(thread_data_t)) != 0)
    {
      perror("posix_memalign");
      exit(1);
    }

  /* Init STM */
  BARRIER;

  data->id = app_id_seq(NODE_ID());
  data->check = check;
  data->read_all = read_all;
  data->write_all = write_all;
  data->read_cores = read_cores;
  data->write_cores = write_cores;
  data->disjoint = disjoint;
  data->nb_app_cores = nb_app_cores;
  data->nb_transfer = 0;
  data->nb_read_all = 0;
  data->nb_write_all = 0;

  BARRIER;

  bank = test(data, duration, nb_accounts);

  /* End it */

  BARRIER;

  uint32_t nd;
  for (nd = 0; nd < TOTAL_NODES(); nd++) {
    if (NODE_ID() == nd) {
      printf("---Core %d\n  #transfer   : %u\n  #checks     : %u\n  #read-all   : %u\n  #write-all  : %u\n", 
	     NODE_ID(), data->nb_transfer, data->nb_checks, data->nb_read_all, data->nb_write_all);
      FLUSH;
    }
    BARRIER;
  }


  BARRIER;

  ONCE
    {
      PRINT("\t\t\t~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
	    "\t\t\t\tBank total (after): %d\n"
	    "\t\t\t~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
	    total(bank, 0));
    }

  /* Delete bank and accounts */

  sys_shfree((volatile unsigned char *) bank->accounts);
  free(bank);
  free(data);

  TM_END;

  EXIT(0);
}
