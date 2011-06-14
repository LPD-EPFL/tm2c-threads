/*
 *  intset.c
 *  
 *  Integer set operations (contain, insert, delete) 
 *  that call stm-based / lock-free counterparts.
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */


//#define STM

#define SEQUENTIAL

#include "intset.h"

#define SET             set
#define ND(offs)        O2N(SET, (offs))
#define OF(node)        N2O(SET, (node))

int set_contains(intset_t *set, val_t val, int transactional) {
    int result;

#ifdef DEBUG
    printf("++> set_contains(%d)\n", (int) val);
    FLUSH;
#endif

#ifdef SEQUENTIAL
    node_t *prev, *next;

    prev = set->head;
    next = ND(prev->next);
    while (next->val < val) {
        prev = next;
        next = ND(prev->next);
    }
    result = (next->val == val);

#elif defined STM			

    node_t *prev, *next;
    val_t v = 0;

    TX_START
    prev = set->head;
    next = ND(*(nxt_t *) TX_LOAD(&prev->next));
    while (1) {
        v = *(val_t *) TX_LOAD(&next->val);
        if (v >= val)
            break;
        prev = next;
        next = ND(*(nxt_t *) TX_LOAD(&prev->next));
    }
    TX_COMMIT
    result = (v == val);

#elif defined LOCKFREE			
    result = harris_find(set, val);
#endif	

    return result;
}

inline int set_seq_add(intset_t *set, val_t val) {
    int result;
    node_t *prev, *next;

    prev = set->head;
    next = ND(prev->next);
    while (next->val < val) {
        prev = next;
        next = ND(prev->next);
    }
    result = (next->val != val);
    if (result) {
        prev->next = OF(new_node(val, OF(next), 0));
    }
    return result;
}

int set_add(intset_t *set, val_t val, int transactional) {
    int result = 0;

#ifdef DEBUG
    printf("++> set_add(%d)\n", (int) val);
    IO_FLUSH;
#endif

    if (!transactional) {

        result = set_seq_add(set, val);

    }
    else {

#ifdef SEQUENTIAL /* Unprotected */

        result = set_seq_add(set, val);

#elif defined STM

        node_t *prev, *next;
        val_t v;
        TX_START
        prev = set->head;
        next = ND(*(nxt_t *) TX_LOAD(&prev->next));
        while (1) {
            v = *(val_t *) TX_LOAD(&next->val);
            if (v >= val)
                break;
            prev = next;
            next = ND(*(nxt_t *) TX_LOAD(&prev->next));
        }
        result = (v != val);
        if (result) {
            nxt_t nxt = OF(new_node(val, OF(next), transactional));
            TX_STORE(&prev->next, &nxt, TYPE_UINT);
        }
        TX_COMMIT

#elif defined LOCKFREE
        result = harris_insert(set, val);
#endif

    }

    return result;
}



int set_remove(intset_t *set, val_t val, int transactional) {
    int result = 0;

#ifdef DEBUG
    printf("++> set_remove(%d)\n", (int) val);
    IO_FLUSH;
#endif

#ifdef SEQUENTIAL /* Unprotected */

    node_t *prev, *next;

    prev = set->head;
    next = ND(prev->next);
    while (next->val < val) {
        prev = next;
        next = ND(prev->next);
    }
    result = (next->val == val);
    if (result) {
        node_t * t = ND(prev->next);
        t = ND(next->next);
        free(next);
    }

#elif defined STM

    node_t *prev, *next;
    val_t v;
    node_t *n;
    TX_START
    prev = set->head;
    next = ND(*(nxt_t *) TX_LOAD(&prev->next));
    while (1) {
        v = *(val_t *) TX_LOAD(&next->val);
        if (v >= val)
            break;
        prev = next;
        next = ND(*(nxt_t *) TX_LOAD(&prev->next));
    }
    result = (v == val);
    if (result) {
        n = ND(*(nxt_t *) TX_LOAD(&next->next));
        TX_STORE(&prev->next, OF(n), TYPE_UINT);
        TX_SHFREE(next);
    }
    TX_COMMIT

#elif defined LOCKFREE
    result = harris_delete(set, val);
#endif

    return result;
}


