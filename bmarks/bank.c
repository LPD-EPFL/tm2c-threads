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


/*       PRINT("sizeof(size_t) = %d", sizeof(size_t)); */
/*       PRINT("sizeof(int_reg_t) = %d", sizeof(int_reg_t)); */
/*       PRINT("sizeof(uintptr_t) = %d", sizeof(uintptr_t)); */
/*       PRINT("sizeof(PS_COMMAND) = %d", sizeof(PS_COMMAND)); */
/*       PRINT("sizeof(PS_STATS_CMD_T) = %d", sizeof(PS_STATS_CMD_T)); */
/*       PRINT("sizeof(PS_REPLY) = %d", sizeof(PS_REPLY)); */
/*       PRINT("sizeof(stm_tx_t) = %d", sizeof(stm_tx_t)); */
/*       PRINT("sizeof(stm_tx_node_t) = %d", sizeof(stm_tx_node_t)); */
/*       PRINT("sizeof(sigjmp_buf) = %d", sizeof(sigjmp_buf)); */
