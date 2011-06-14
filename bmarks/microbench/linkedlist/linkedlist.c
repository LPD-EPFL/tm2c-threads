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

node_t *new_node(val_t val, nxt_t next, int transactional) {
    node_t *node;

    if (transactional) {
        node = (node_t *) TX_SHMALLOC(sizeof (node_t));
    }
    else {
        node = (node_t *) RCCE_shmalloc(sizeof (node_t));
    }
    if (node == NULL) {
        perror("malloc");
        EXIT(1);
    }

    node->val = val;
    node->next = next;

    return node;
}

intset_t *set_new() {
    intset_t *set;
    node_t *min, *max;

    if ((set = (intset_t *) RCCE_shmalloc(sizeof (intset_t))) == NULL) {
        perror("malloc");
        EXIT(1);
    }
    max = new_node(VAL_MAX, NULL, 0);
    min = new_node(VAL_MIN, N2O(set, max), 0);
    set->head = min;

    return set;
}

void set_delete(intset_t *set) {
    node_t *node, *next;

    node = set->head;
    while (node != NULL) {
        next = O2N(set, node->next);
        free(node);
        node = next;
    }
    free(set);
}

int set_size(intset_t *set) {
    int size = 0;
    node_t *node;

    /* We have at least 2 elements */
    node = O2N(set, set->head->next);
    while (node->nextp != NULL) {
        size++;
        node = O2N(set, node->next);
    }

    return size;
}
