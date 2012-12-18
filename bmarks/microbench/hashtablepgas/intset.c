/*
 * File:
 *   intset.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Integer set operations accessing the hashtable
 *
 * Copyright (c) 2009-2010.
 *
 * intset.c is part of Microbench
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

#include "intset.h"

intset_t *offset;

int 
ht_contains(ht_intset_t* set, int val, int transactional) 
{
  int addr, result;

  addr = val % maxhtlength;
  result = set_contains(set->buckets[addr], val, transactional);

  return result;
}

int 
ht_add(ht_intset_t* set, int val, int transactional) 
{
  int addr, result;

  addr = val % maxhtlength;
  result = set_add(set->buckets[addr], val, transactional);

  return result;
}

int 
ht_remove(ht_intset_t* set, int val, int transactional) 
{
  int addr, result;

  addr = val % maxhtlength;
  result = set_remove(set->buckets[addr], val, transactional);

  return result;
}

/* 
 * Move an element from one bucket to another.
 * It is equivalent to changing the key associated with some value.
 *
 * This version allows the removal of val1 while insertion of val2 fails (e.g.
 * because val2 already present. (Despite this partial failure, the move returns 
 * true.) As a result, data structure size may decrease as moves execute.
 */
int ht_move_naive(ht_intset_t *set, int val1, int val2, int transactional) {

  int result = 0;

#ifdef IMPLEMENTED /*-- not implemented*/
#ifdef SEQUENTIAL

    int addr1, addr2;

    addr1 = val1 % maxhtlength;
    addr2 = val2 % maxhtlength;
    result = (set_remove(set->buckets[addr1], val1, transactional) &&
            set_add(set->buckets[addr2], val2, transactional));

#elif defined STM

    int v, addr1, addr2;
    node_t prev, next;
    pgas_addr_t prev_addr;

    TX_START
    addr1 = val1 % maxhtlength;
    prev_addr = set->buckets[addr1]->head;
    prev = (node_t) TX_LOAD(prev_addr);
    next = (node_t) TX_LOAD(prev.next);
    while (1) {
        v = next.val; //was TX
        if (v >= val1) {
            break;
        }
        prev_addr = prev.next;
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
    }
    if (v == val1) {
        /* Physically removing */
        node_t prevnew = prev;
        prevnew.next = next.next;
        prevnew.val = prev.val;
        TX_STORE(prev_addr, next.next);
        /* Inserting */
        addr2 = val2 % maxhtlength;
        prev_addr = set->buckets[addr2]->head;
        prev = (node_t) TX_LOAD(prev_addr);
        next = (node_t) TX_LOAD(prev.next);
        while (1) {
            v = next.val; //was TX
            if (v >= val2) {
                break;
            }
            prev_addr = prev.next;
            prev = next;
            next = (node_t) TX_LOAD(prev.next);
        }
        if (v != val2) {

            new_node_t nn = new_node(val2, prev.next, 1);
            node_t prevnew = prev;
            prevnew.next = nn.addr;
            prevnew.val = prev.val;
            TX_STORE(prev_addr, prevnew.toint);
            TX_STORE(nn.addr, nn.node.toint);

            //PRINTD("Created node %5d. Value: %d", nxt, val);
        }
        /* Even if the key is already in, the operation succeeds */
        result = 1;
    }
    else result = 0;
    TX_COMMIT

#endif

#endif /*implemented*/
    return result;


}

/*
 * This version parses the data structure twice to find appropriate values 
 * before updating it.
 */
int ht_move(ht_intset_t *set, int val1, int val2, int transactional) {
    int result = 0;

#ifdef IMPLEMENTED /*-- not implemented*/
#ifdef SEQUENTIAL

    int addr1, addr2;

    addr1 = val1 % maxhtlength;
    addr2 = val2 % maxhtlength;
    //result =  (set_remove(set->buckets[addr1], val1, transactional) && 
    //		   set_add(set->buckets[addr2], val2, transactional));

    if (set_remove(set->buckets[addr1], val1, 0))
        result = 1;
    set_add(set->buckets[addr2], val2, 0);

    return result;

#elif defined STM

    int v, addr1, addr2;
    node_t prev, next, prev1, next1;
    pgas_addr_t prev_addr, prev1_addr;

    TX_START
    addr1 = val1 % maxhtlength;

    prev_addr = set->buckets[addr1]->head;
    prev = (node_t) TX_LOAD(prev_addr);
    next = (node_t) TX_LOAD(prev.next);
    while (1) {
        v = next.val; //was TX
        if (v >= val1) {
            break;
        }
        prev_addr = prev.next;
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
    }
    prev1 = prev;
    prev1_addr = prev_addr;
    next1 = next;
    if (v == val1) {
        /* Inserting */
        addr2 = val2 % maxhtlength;
        prev_addr = set->buckets[addr2]->head;
        prev = (node_t) TX_LOAD(prev_addr);
        next = (node_t) TX_LOAD(prev.next);

        while (1) {
            v = next.val; //was TX
            if (v >= val2) break;
            prev_addr = prev.next;
            prev = next;
            next = (node_t) TX_LOAD(prev.next);
        }

        if (v != val2 && prev.val != prev1.val && prev.val != next1.val) {
            /* Even if the key is already in, the operation succeeds */
            result = 1;

            /* Physically removing */
            node_t prevnew = prev1;
            prevnew.next = next1.next;
            prevnew.val = prev1.val;
            TX_STORE(prev1_addr, next1.next);

            new_node_t nn = new_node(val2, prev.next, 1);
            prevnew = prev;
            prevnew.next = nn.addr;
            prevnew.val = prev.val;
            TX_STORE(prev_addr, prevnew.toint);
            TX_STORE(nn.addr, nn.node.toint);

        }
    }
    else result = 0;
    TX_COMMIT

#endif
#endif /*implemented*/
    return result;
}

/*
 * This version removes val1 it finds first and re-insert this value if it does 
 * not succeed in inserting val2 so that it can insert somewhere (for the size 
 * to remain unchanged).
 */
/*
int ht_move_orrollback(ht_intset_t *set, int val1, int val2, int transactional) {
    int result = 0;

#ifdef SEQUENTIAL

    int addr1, addr2;


#ifdef LOCKS
    global_lock();
#endif 

    addr1 = val1 % maxhtlength;
    addr2 = val2 % maxhtlength;
    result = (set_remove(set->buckets[addr1], val1, transactional) &&
            set_add(set->buckets[addr2], val2, transactional));

#ifdef LOCKS
    global_lock_release();
#endif

#elif defined STM

    int v, addr1, addr2;
    node_t *prev, *next, *prev1, *next1;

    TX_START
    addr1 = val1 % maxhtlength;
    OFFSET(set->buckets[addr1]);
    prev = ND(*(nxt_t *) (node_t) TX_LOAD(&set->buckets[addr1]->head));
    next = ND(*(nxt_t *) (node_t) TX_LOAD(&prev->next));
    while (1) {
        v = next->val; //was TX
        if (v >= val1) {
            break;
        }
        prev = next;
        next = ND(*(nxt_t *) (node_t) TX_LOAD(&prev->next));
    }
    prev1 = prev;
    next1 = next;
    if (v == val1) {
        // Physically removing 
        nxt_t *nxt = (nxt_t *) (node_t) TX_LOAD(&next->next);
        TX_STORE(&prev->next, nxt, TYPE_UINT);
        // Inserting 
        addr2 = val2 % maxhtlength;
        OFFSET(set->buckets[addr2]);
        prev = ND(*(nxt_t *) (node_t) TX_LOAD(&set->buckets[addr2]->head));
        next = ND(*(nxt_t *) (node_t) TX_LOAD(&prev->next));
        while (1) {
            v = next->val; //was TX
            if (v >= val2) {
                break;
            }
            prev = next;
            next = ND(*(nxt_t *) (node_t) TX_LOAD(&prev->next));
        }
        if (v != val2) {
            nxt_t nxt = OF(new_node(val2, OF(next), transactional));
            //PRINTD("Created node %5d. Value: %d", nxt, val);
            TX_STORE(&prev->next, &nxt, TYPE_UINT);
            TX_SHFREE(next1);
        }
        else {
            nxt_t *nxt = (nxt_t *) (node_t) TX_LOAD(&next1);
            TX_STORE(&prev1->next, nxt, TYPE_UINT);
        }
        //Even if the key is already in, the operation succeeds
        result = 1;
    }
    else result = 0;
    TX_COMMIT

#endif

    return result;
}
*/

/*
 * Atomic snapshot of the hash table.
 * It parses the whole hash table to sum all elements.
 *
 * Observe that this particular operation (atomic snapshot) cannot be implemented using 
 * elastic transactions in combination with the move operation, however, normal transactions
 * compose with elastic transactions.
 */
int ht_snapshot(ht_intset_t *set, int transactional) {
    int result = 0;

#ifdef IMPLEMENTED /*-- not implemented*/
    int i, sum;
    node_t next;

    TX_START
    sum = 0;
    for (i = 0; i < maxhtlength; i++) {
        node_t hd = (node_t) TX_LOAD(set->buckets[i]->head);
        next = (node_t) TX_LOAD(hd.next);
        while (next.next != 0) {
            sum += next.val;
            next = (node_t) TX_LOAD(next.next);
        }
    }
    TX_COMMIT
    result = 1;
#endif /*implemented*/
    return result;
}
