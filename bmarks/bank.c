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
#define MC 
#endif

#define DEFAULT_DURATION                10
#define DEFAULT_DELAY                   0
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                0
#define DEFAULT_CHECK                   20
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

typedef struct account {
  int32_t number;
  int32_t balance;
} account_t;

typedef struct bank {
    account_t *accounts;
    int size;
} bank_t;

int transfer(account_t *src, account_t *dst, int amount) {
  /* PRINT("in transfer"); */

  int i, j;

  /* PF_START(2); */

  /* Allow overdrafts */
  TX_START;

#ifdef LOAD_STORE
  //TODO: test and use the TX_LOAD_STORE
  /* PF_START(0); */
  TX_LOAD_STORE(&src->balance, -, amount, TYPE_INT);
  /* PF_STOP(0); */
  /* PF_START(1); */
  TX_LOAD_STORE(&dst->balance, +, amount, TYPE_INT);
  /* PF_STOP(1); */
    
  TX_COMMIT_NO_PUB;
#else
  i = *(int *) TX_LOAD(&src->balance);
  i -= amount;
  TX_STORE(&src->balance, &i, TYPE_INT); //NEED TX_STOREI
  j = *(int *) TX_LOAD(&dst->balance);
  j += amount;
  TX_STORE(&dst->balance, &j, TYPE_INT);
  TX_COMMIT;
#endif

  /* PF_STOP(2); */
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
        TX_STORE(&bank->accounts[I(i)].balance, &j, TYPE_INT);
#endif
    }
    TX_COMMIT
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data {
  uint64_t nb_transfer;
  uint64_t nb_checks;
  uint64_t nb_read_all;
  uint64_t nb_write_all;
  int32_t id;
  int32_t read_cores;
  int32_t read_all;
  int32_t check;
  int32_t disjoint;
  int32_t nb_app_cores;
  int32_t padding[2];
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
      rand_max = nb_accounts / d->nb_app_cores;
      rand_min = rand_max * d->id;
      if (rand_max <= 2) {
	fprintf(stderr, "can't have disjoint account accesses");
	return NULL;
      }
    }
  else 
    {
      rand_max = nb_accounts;
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

  // PRINT("chk %d", chk++); //0

  TX_START;
  /*   TX_LOAD(&bank->accounts[0].balance); */
  /* int lol = 0; */
  /* TX_STORE(&bank->accounts[1].balance, &lol, TYPE_INT); */
  TX_COMMIT_NO_STATS;
  total(bank, 0);

  BARRIER;

  FOR(duration) {

    if (d->id < d->read_cores) 
      {
	/* Read all */
	//  PRINT("READ ALL1");
	total(bank, 1);
	d->nb_read_all++;
      }
    else 
      {
	int nb = (int) (rand_range(100) - 1);
	if (nb < d->read_all) 
	  {
	    //     PRINT("READ ALL2");
	    /* Read all */
	    total(bank, 1);
	    d->nb_read_all++;
	  }
	else if (nb < d->check) 
	  {
	    /* Choose random accounts */
	    int src = (int) (rand_range(rand_max) - 1) + rand_min;
	    int dst = (int) (rand_range(rand_max) - 1) + rand_min;
	    if (dst == src)
	      {
		dst = ((src + 1) % rand_max) + rand_min;
	      }
	    check_accs(&bank->accounts[src], &bank->accounts[dst]);

	    d->nb_checks++;
	  }
	else 
	  {
	    /* Choose random accounts */
	    int src = (int) (rand_range(rand_max) - 1) + rand_min;
	    int dst = (int) (rand_range(rand_max) - 1) + rand_min;
	    if (dst == src)
	      {
		dst = ((src + 1) % rand_max) + rand_min;
	      }

#ifndef PLATFORM_CLUSTER
	    transfer(&bank->accounts[I(src)], &bank->accounts[I(dst)], 1);
#else
	    transfer(&bank->accounts[src], &bank->accounts[dst], 1);
#endif

	    d->nb_transfer++;
	  }
      }

    if (delay) 
      {
	udelay(rand_range(delay));
      }
  } END_FOR;

  //reset(bank);

  BARRIER;

  return bank;
}

TASKMAIN(int argc, char **argv) {
  dup2(STDOUT_FILENO, STDERR_FILENO);

  PF_MSG(0, "1st TX_LOAD_STORE (transfer)");
  PF_MSG(1, "2nd TX_LOAD_STORE (transfer)");
  PF_MSG(2, "the whole transfer");

  TM_INIT

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
  int check = DEFAULT_CHECK;
  int read_all = DEFAULT_READ_ALL;
  int read_cores = DEFAULT_READ_THREADS;
  int write_all = DEFAULT_WRITE_ALL;
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
		"        Delay after succesfull request. Used to understress the system, default=" XSTR(DEFAULT_DELAY) ")\n"
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
      break;
    case 'W':
      write_cores = atoi(optarg);
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


  assert(duration >= 0);
  assert(nb_accounts >= 2);
  assert(nb_app_cores > 0);
  assert(read_all >= 0 && write_all >= 0 && read_all + write_all <= 100);
  assert(read_all >= 0 && write_all >= 0 && check >= 0 && read_all + write_all + check <= 100);
  assert(read_cores + write_cores <= nb_app_cores);

  ONCE
    {
      PRINTN("Nb accounts    : %d\n", nb_accounts);
      PRINTN("Duration       : %fs\n", duration);
      PRINTN("Nb cores       : %d\n", nb_app_cores);
      PRINTN("Check acc rate : %d\n", check);
      PRINTN("Transfer rate  : %d\n", 100 - check - read_all - write_all);
      PRINTN("Read-all rate  : %d\n", read_all);
      PRINTN("Read cores     : %d\n", read_cores);
      PRINTN("Write-all rate : %d\n", write_all);
      PRINTN("Write cores    : %d\n", write_cores);

      PRINT("sizeof(..) = %lu", sizeof(thread_data_t));
    }

  
  if (posix_memalign((void**) &data, 64, sizeof(thread_data_t)) != 0)
    {
      perror("posix_memalign");
      exit(1);
    }

  /* Init STM */
  BARRIER;

  data->id = app_id_seq(NODE_ID());
  data->check = read_all + check;
  data->read_all = read_all;
  data->read_cores = read_cores;
  data->disjoint = disjoint;
  data->nb_app_cores = nb_app_cores;
  data->nb_transfer = 0;
  data->nb_read_all = 0;
  data->nb_write_all = 0;

  BARRIER;

  bank = test(data, duration, nb_accounts);

  /* End it */

  BARRIER

    uint32_t nd;
  for (nd = 0; nd < TOTAL_NODES(); nd++) {
    if (NODE_ID() == nd) {
      printf("---Core %d\n  #transfer   : %lu\n  #checks     : %lu\n  #read-all   : %lu\n  #write-all  : %lu\n", 
	     NODE_ID(), data->nb_transfer, data->nb_checks, data->nb_read_all, data->nb_write_all);
      FLUSH;
    }
    BARRIER;
  }


  BARRIER

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

