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

pgas_addr_t new_node(val_t val, nxt_t next, int transactional) {
    pgas_addr_t addr;

    if (transactional) {
        addr = PGAS_alloc();
    }
    else {
        addr = PGAS_alloc_seq();
    }

    node_t node;
    node.val = val;
    node.next = next;
    TX_START
    TX_STORE(addr, node.toint);
    TX_COMMIT

    return addr;
}

intset_t *set_new() {
    intset_t *set;
    pgas_addr_t min, max;

    if ((set = (intset_t *) malloc(sizeof (intset_t))) == NULL) {
        perror("malloc");
        EXIT(1);
    }
    PGAS_alloc_init(1);
    PRINT("creating the leftmost/rightmost nodes for new set");
    max = new_node(VAL_MAX, 0, 0);
    PRINT("max is : %d", max);
    min = new_node(VAL_MIN, max, 0);
    PRINT("min is : %d", min);
    set->head = min;
    return set;
}

void set_delete(intset_t *set) {
    free(set);
}

int set_size(intset_t *set) {
    int size;
    node_t node, head;
    /* We have at least 2 elements */
    TX_START
    size = 0;
    head.next = set->head;
    node = (node_t) TX_LOAD(head.next);
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

    return result;
}

static int set_seq_add(intset_t *set, val_t val) {
    int result;
    node_t prev, next;

    TX_START
    prev.next = set->head;
    next = (node_t) TX_LOAD(prev.next);
    PRINT("set->head: %d, next.next: %d", set->head, next.next);
    while (next.val < val) {
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
        PRINT("next : (%d)(%d)", next.val, next.next);
    }
    result = (next.val != val);
    if (result) {
        TX_STORE(prev.next, new_node(val, next.next, 0));
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

    val_t v;
    node_t prev, next;
    TX_START
    prev = (node_t) TX_LOAD(set->head);
    next = (node_t) TX_LOAD(prev.next);

    v = next.val;
    if (v >= val)
        goto done;

    prev = next;
    next = (node_t) TX_LOAD(prev.next);

    while (1) {
        v = next.val;
        if (v >= val)
            break;
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
    }
done:
    result = (v != val);
    if (result) {
        pgas_addr_t nxt = new_node(val, next.next, transactional);
        PRINTD("Created node %5d. Value: %d", nxt, val);
        TX_STORE(prev.next, nxt);
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

    TX_START
    prev = (node_t) TX_LOAD(set->head);
    next = (node_t) TX_LOAD(prev.next);

    v = next.val;
    if (v >= val)
        goto done;

    prev = next;
    next = (node_t) TX_LOAD(prev.next);

    while (1) {
        v = next.val;
        if (v >= val)
            break;
        prev = next;
        next = (node_t) TX_LOAD(prev.next);
    }
done:
    result = (v == val);
    if (result) {
        node_t nxt = (node_t) TX_LOAD(next.next);
        TX_STORE(prev.next, nxt.next);
        PRINTD("Freed node   %5d. Value: %d", next, next.val);
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
        printf("%d -> ", node.val);
        node = (node_t) TX_LOAD(node.next);
    }
    TX_COMMIT
    null :
            PRINTSF("NULL\n");

}