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

#define DEFAULT_DURATION                10
#define DEFAULT_INITIAL                 10
//256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   10
//0x7FFFFFFF
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY							4
#define DEFAULT_ALTERNATE								0
#define DEFAULT_EFFECTIVE								1

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define TRANSACTIONAL                   d->unit_tx

typedef intptr_t val_t;
typedef unsigned int nxt_t;

#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX

typedef struct node {
	val_t val;
        union {
                struct node *nextp;
                nxt_t next;
        };
} node_t;

typedef struct intset {
	node_t *head;
} intset_t;

#define N2O(set, node)                    (nxt_t) ((nxt_t) (node) - (nxt_t) (set))
#define O2N(set, offset)                  ((void *) (offset) == NULL ? NULL : (node_t *) ((nxt_t) (set) + (offset)))


node_t *new_node(val_t val, nxt_t next, int transactional);
intset_t *set_new();
void set_delete(intset_t *set);
int set_size(intset_t *set);

/*intset.c*/
int set_contains(intset_t *set, val_t val, int transactional);
int set_add(intset_t *set, val_t val, int transactional);
int set_remove(intset_t *set, val_t val, int transactional);

void set_print(intset_t *set);