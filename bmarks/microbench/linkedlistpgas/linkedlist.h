/*
 *  linkedlist.h
 *  
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include "tm.h"

/*______________________________________________________________________________
 * SETTINGS
 * _____________________________________________________________________________
 */
#define SEQUENTIAL_
#define LOCKS_
#define STM
#define EARLY_RELEASE_
#define READ_VALIDATION_

#ifdef READ_VALIDATION
#ifdef EARLY_RELEASE
#undef EARLY_RELEASE
#endif
#endif

#define DEFAULT_DURATION                10
#define DEFAULT_INITIAL                 256
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY		4
#define DEFAULT_ALTERNATE		0
#define DEFAULT_EFFECTIVE		1
#define DEFAULT_LOCKS                   0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define TRANSACTIONAL                   1

#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX

typedef uint32_t val_t;
typedef uint32_t nxt_t;

typedef struct node 
{
  union
  {
    struct
    {
      val_t val;
      nxt_t next;
    };
    int64_t to_int64;
  };
} node_t;

typedef struct intset 
{
  node_t* head;
} intset_t;

extern nxt_t offs__;

#define ND(offs)                        pgas_app_addr_from_offs(offs)
#define OF(node)                        pgas_app_addr_offs(node)
#define LOAD_NODE(nd, addr)                     \
  nd.to_int64 = NONTX_LOAD(addr, 2)

#define LOAD_NODE_NXT(nd, addr)                 \
  ;//nd.next = (nxt_t) NONTX_LOAD((tm_addr_t)((size_t)addr + sizeof(sys_t_vcharp)))

#define TX_LOAD_NODE(nd, addr)                  \
  nd.to_int64 = TX_LOAD(addr, 2); //PRINT("       load: %-13u = %-13u -> %u", OF(addr), nd.val, nd.next);
#define TX_LOAD_NODE_NXT(nd, addr)              \
  ;/* nd.next = (nxt_t) TX_LOAD((tm_addr_t)((size_t)addr + sizeof(sys_t_vcharp))) */

node_t *new_node(val_t val, nxt_t next, int transactional);
intset_t *set_new();
void set_delete(intset_t *set);
int set_size(intset_t *set);

/*intset.c*/
int set_contains(intset_t *set, val_t val, int transactional);
int set_add(intset_t *set, val_t val, int transactional);
int set_remove(intset_t *set, val_t val, int transactional);

void set_print(intset_t *set);

INLINED void global_lock() {
    PRINTD("askking for global lock");
    RCCE_acquire_lock(0);
    PRINTD("got global lock");
}

INLINED void global_lock_release() {
    RCCE_release_lock(0);
    PRINTD("released global lock");
}
