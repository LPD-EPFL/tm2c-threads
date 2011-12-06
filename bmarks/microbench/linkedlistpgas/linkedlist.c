/*
 *  linkedlist.c
 *  
 *  Linked list data structure
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "linkedlist.h"

void *shmem_init(size_t offset) {
    return (void *) (RCCE_shmalloc(offset) + offset);
}

new_node_t new_node(val_t val, nxt_t next, int transactional) {
    new_node_t nn;

    if (transactional) {
        nn.addr = PGAS_alloc();
    }
    else {
        nn.addr = PGAS_alloc_seq();
    }

    nn.node.val = val;
    nn.node.next = next;

    return nn;
}

intset_t *set_new() {
    intset_t *set;
    pgas_addr_t min, max;

    if ((set = (intset_t *) malloc(sizeof (intset_t))) == NULL) {
        perror("malloc");
        EXIT(1);
    }
    TX_START
    PGAS_alloc_init(1);
    PRINT("creating the leftmost/rightmost nodes for new set");
    new_node_t max_nn = new_node(VAL_MAX, 0, 0);
    PRINT("max is : %d, value: %d, nxt: %d", max_nn.addr, max_nn.node.val, max_nn.node.next);
    new_node_t min_nn = new_node(VAL_MIN, max_nn.addr, 0);
    PRINT("min is : %d, value: %d, nxt: %d", min_nn.addr, min_nn.node.val, min_nn.node.next);
    TX_STORE(max_nn.addr, max_nn.node.toint);
    TX_STORE(min_nn.addr, min_nn.node.toint);
    set->head = min_nn.addr;
    TX_COMMIT
    return set;
}

void set_delete(intset_t *set) {
    free(set);
}

int set_size(intset_t *set) {
    int size;
    node_t node;
    PRINT("SET_SIZE");
    /* We have at least 2 elements */
    TX_START
    size = -1;
    node = (node_t) TX_LOAD(set->head);
    while (node.next != 0) {
        size++;
        node = (node_t) TX_LOAD(node.next);
    }
    TX_COMMIT
    return size;
}

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

int set_contains(intset_t *set, val_t val, int transactional) {
    int result;

#ifdef DEBUG
    printf("++> set_contains(%d)\n", (int) val);
    FLUSH;
#endif

    val_t v = 0;
    node_t prev, next;

    TX_START
    prev.next = set->head;
    next = (node_t) TX_LOAD(prev.next);
    while (1) {
        v = next.val;
        if (v >= val)
            break;
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
    }
    TX_COMMIT
    result = (v == val);

    PRINT("contains %d (%d)", val, result);
    return result;
}

static int set_seq_add(intset_t *set, val_t val) {
    int result;
    node_t prev, next;
    pgas_addr_t prev_addr;

    TX_START
    prev = (node_t) TX_LOAD(set->head);
    prev_addr = set->head;
    next = (node_t) TX_LOAD(prev.next);
    while (next.val < val) {
        prev_addr = prev.next;
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
        PRINT("%d:%d", prev.next, next.val);
    }
    result = (next.val != val);
    if (result) {
        new_node_t nn = new_node(val, prev.next, 0);
        PRINT("adding value %d addr %d, after %d, before %d", nn.node.val, nn.addr, prev.val, next.val);
        node_t prevnew = prev;
        prevnew.next = nn.addr;
        prevnew.val = prev.val;
        TX_STORE(prev_addr, prevnew.toint);
        TX_STORE(nn.addr, nn.node.toint);
    }
    TX_COMMIT
    return result;
}

int set_add(intset_t *set, val_t val, int transactional) {
    int result = 0;

#ifdef DEBUG
    printf("++> set_add(%d)\n", (int) val);
    FLUSH;
#endif

    if (!transactional) {
        return set_seq_add(set, val);
    }

    node_t prev, next;
    pgas_addr_t prev_addr;

    TX_START
    prev = (node_t) TX_LOAD(set->head);
    prev_addr = set->head;
    next = (node_t) TX_LOAD(prev.next);
    while (next.val < val) {
        prev_addr = prev.next;
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
        PRINT("%d:%d", prev.next, next.val);
    }
    result = (next.val != val);
    if (result) {
        new_node_t nn = new_node(val, prev.next, 1);
        PRINT("adding value %d addr %d, after %d, before %d", nn.node.val, nn.addr, prev.val, next.val);
        node_t prevnew = prev;
        prevnew.next = nn.addr;
        prevnew.val = prev.val;
        TX_STORE(prev_addr, prevnew.toint);
        TX_STORE(nn.addr, nn.node.toint);
    }
    TX_COMMIT

    return result;
}

int set_remove(intset_t *set, val_t val, int transactional) {
    int result = 0;

#ifdef DEBUG
    printf("++> set_remove(%d)\n", (int) val);
    FLUSH;
#endif

    val_t v;
    node_t prev, next;
    pgas_addr_t prev_addr;

    TX_START
    prev = (node_t) TX_LOAD(set->head);
    prev_addr = set->head;
    next = (node_t) TX_LOAD(prev.next);

    v = next.val;
    if (v >= val)
        goto done;

    prev_addr = prev.next;
    prev = next;
    next = (node_t) TX_LOAD(prev.next);
    while (1) {
        v = next.val;
        if (v >= val)
            break;
        prev_addr = prev.next;
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
    }
done:
    result = (v == val);
    if (result) {
        PRINT("removing value %d addr %d, before %d", val, prev.next, next.next);
        node_t prevnew = prev;
        prevnew.next = next.next;
        prevnew.val = prev.val;
        TX_STORE(prev_addr, prevnew.toint);
        PRINTD("Freed node   %5d. Value: %d", prev.next, next.val);
    }
    TX_COMMIT

    return result;
}

void set_print(intset_t* set) {

    TX_START
    node_t node = (node_t) TX_LOAD(set->head);

    if (node.next == NULL) {
        goto null;
    }
    while (node.next != NULL) {
        printf("{%d} -%d-> ", node.val, node.next);
        node = (node_t) TX_LOAD(node.next);
    }
    TX_COMMIT
    null :
            PRINTSF("NULL\n");

}