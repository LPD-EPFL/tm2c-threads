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

#define I(a) to_intern_addr(a)
nxt_t offs__;

void *shmem_init(size_t offset) {
    return (void *) (sys_shmalloc(offset) + offset);
}

node_t *new_node(val_t val, nxt_t next, int transactional) {
    node_t *node;

    if (transactional) {
        node = (node_t *) TX_SHMALLOC(sizeof (node_t));
    }
    else {
        node = (node_t *) sys_shmalloc(sizeof (node_t));
    }
    if (node == NULL) {
        perror("malloc");
        EXIT(1);
    }

    NONTX_STORE(&node->val, val, TYPE_INT);
    NONTX_STORE(&node->next, next, TYPE_INT);

    return node;
}

intset_t *set_new() {
    intset_t *set;
    node_t *min, *max;

    if ((set = (intset_t *) malloc(sizeof (intset_t))) == NULL) {
        perror("malloc");
        EXIT(1);
    }
    max = new_node(VAL_MAX, 0, 0);
    min = new_node(VAL_MIN, (nxt_t) max, 0);
    set->head = (nxt_t) min;
    return set;
}

void set_delete(intset_t *set) {
    node_t node, next;

    LOAD_NODE_NXT(node, set->head);
    nxt_t to_del = set->head;
    while (node.next != 0) {
        to_del = node.next;
        LOAD_NODE_NXT(next, node.next);
        sys_shfree((t_vcharp) to_del);
        node.next = next.next;
    }
    sys_shfree((t_vcharp) set);
}

int set_size(intset_t *set) {
    int size = 0;
    node_t node, head;

    /* We have at least 2 elements */
    LOAD_NODE_NXT(head, set->head);
    LOAD_NODE_NXT(node, head.next);
    while (node.nextp != NULL) {
        size++;
        LOAD_NODE_NXT(node, node.next);
    }

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

/*
  set contains --------------------------------------------------

 */
static int set_seq_contains(intset_t *set, int val);
static int set_early_contains(intset_t *set, int val);
static int set_readval_contains(intset_t *set, int val);

int set_contains(intset_t *set, val_t val, int transactional) {
    int result;

#ifdef DEBUG
    printf("++> set_contains(%d)\n", (int) val);
    FLUSH;
#endif

#ifdef SEQUENTIAL
    return set_seq_contains(set, val);
#endif
#ifdef EARLY_RELEASE
    return set_early_contains(set, val):
#endif

#ifdef READ_VALIDATION
      return set_readval_contains(set, val);
#endif

    node_t prev, next;
    val_t v = 0;

    TX_START
    LOAD_NODE_NXT(prev, set->head);
    TX_LOAD_NODE(next, prev.next);
    while (1) {
        v = next.val;
        if (v >= val)
            break;
        prev.next = next.next;
        TX_LOAD_NODE(next, prev.next);
    }
    TX_COMMIT
    result = (v == val);

    return result;
}

static int set_seq_contains(intset_t *set, val_t val) {
    int result;

    node_t prev, next;

#ifdef LOCKS
    global_lock();
#endif

    /* We have at least 2 elements */
    LOAD_NODE_NXT(prev, set->head);
    LOAD_NODE(next, prev.next);
    while (next.val < val) {
      prev.next = next.next;
      LOAD_NODE(next, prev.next);
    }

    result = (next.val == val);

#ifdef LOCKS
    global_lock_release();
#endif

    return result;
}

static int set_early_contains(intset_t *set, val_t val) {
  int result;
  node_t prev, next;
#ifdef EARLY_RELEASE
    node_t *rls;
#endif
    val_t v = 0;

    TX_START
    LOAD_NODE_NXT(prev, set->head);
    TX_LOAD_NODE(next, prev.next);
    while (1) {
        v = next.val;
        if (v >= val)
            break;
#ifdef EARLY_RELEASE
        rls = prev;
#endif
        prev.next = next.next;
        TX_LOAD_NODE(next, prev.next);
#ifdef EARLY_RELEASE
        TX_RRLS(&rls->next);
#endif
    }
    TX_COMMIT
    result = (v == val);

    return result;
}

static int set_readval_contains(intset_t *set, val_t val) {
  int result;

  node_t *prev, *next, *validate;
  nxt_t nextoffs, validateoffs;
  val_t v = 0;

  TX_START
    prev = ND(set->head);
  nextoffs = prev->next;
  next = ND(nextoffs);
  validate = prev;
  validateoffs = nextoffs;
  while (1) {
    v = next->val;
    if (v >= val)
      break;
    validate = prev;
    validateoffs = nextoffs;
    prev = next;
    nextoffs = prev->next;
    next = ND(nextoffs);
    if (validate->next != validateoffs) {
      PRINTD("[C1] Validate failed: expected nxt: %d, got %d", validateoffs, validate->next);
      TX_ABORT(READ_AFTER_WRITE);
    }
  }
  if (validate->next != validateoffs) {
    PRINTD("[C2] Validate failed: expected nxt: %d, got %d", validateoffs, validate->next);
    TX_ABORT(READ_AFTER_WRITE);
  }
  TX_COMMIT
    result = (v == val);

  return result;
}



/*
  set add ----------------------------------------------------------------------                                                                                 

 */

static int set_seq_add(intset_t *set, val_t val);
static int set_early_add(intset_t *set, val_t val);
static int set_readval_add(intset_t *set, val_t val);

int set_add(intset_t *set, val_t val, int transactional) {
    int result = 0;

#ifdef DEBUG
    printf("++> set_add(%d)\n", (int) val);
    FLUSH;
#endif

    if (!transactional) {
        return set_seq_add(set, val);
    }

#ifdef SEQUENTIAL /* Unprotected */
    return set_seq_add(set, val);
#endif
#ifdef EARLY_RELEASE
    return set_early_add(set, val);
#endif

#ifdef READ_VALIDATION
    return set_readval_add(set, val);
#endif
    node_t prev, next;

    val_t v;
    TX_START
      nxt_t to_store = set->head + sizeof(int);
    LOAD_NODE_NXT(prev, set->head);
    LOAD_NODE(next, prev.next);

    v = next.val;
    if (v >= val) {
      goto done;
    }


    to_store = prev.next + sizeof(int);
    prev.next = next.next;
    TX_LOAD_NODE(next, prev.next);

    while (1) {
      v = next.val;
      if (v >= val) {
	break;
      }

      to_store = prev.next + sizeof(int);
      prev.next = next.next;
      LOAD_NODE(next, prev.next);
    }

 done:
    result = (v != val);
    if (result) {
      
      node_t *nn = new_node(val, prev.next, 1);
      //      PRINT("Created node %5d. Value: %d", I(nn), val);
      TX_STORE(to_store, nn, TYPE_INT);
    }
    TX_COMMIT

      return result;
}

static int set_seq_add(intset_t *set, val_t val) {
    int result;
    node_t prev, next;

#ifdef LOCKS
    global_lock();
#endif
    nxt_t to_store = set->head + sizeof(int);
    LOAD_NODE_NXT(prev, set->head);
    LOAD_NODE(next, prev.next);
    int iii = 0;
    while (next.val < val) {
      to_store = prev.next + sizeof(int);
        prev.next = next.next;
        LOAD_NODE(next, prev.next);
	
    }
    result = (next.val != val);
    if (result) {
      node_t *nn = new_node(val, prev.next, 0);
      NONTX_STORE(to_store, nn, TYPE_INT);
    }

#ifdef LOCKS
    global_lock_release();
#endif

    return result;
}

int set_early_add(intset_t *set, int val) {
  int result;

    node_t prev, next;

#ifdef EARLY_RELEASE
    node_t *prls, *pprls;
#endif
    val_t v;
    TX_START
    nxt_t to_store = set->head + sizeof(int);
    LOAD_NODE_NXT(prev, set->head);
    LOAD_NODE(next, prev.next);

    v = next.val;
    if (v >= val) {
        goto done;
    }

#ifdef EARLY_RELEASE
    pprls = prev;
#endif

    to_store = prev.next + sizeof(int);
    prev.next = next.next;
    TX_LOAD_NODE(next, prev.next);

#ifdef EARLY_RELEASE
    prls = prev;
#endif
    while (1) {
      v = next.val;
      if (v >= val) {
	break;
      }

      to_store = prev.next + sizeof(int);
      prev.next = next.next;
      LOAD_NODE(next, prev.next);
#ifdef EARLY_RELEASE
        TX_RRLS(&pprls->next);
        pprls = prls;
        prls = prev;
#endif
    }

done:
    result = (v != val);
    if (result) {
      
      node_t *nn = new_node(val, prev.next, 1);
      //      PRINT("Created node %5d. Value: %d", I(nn), val);
      TX_STORE(to_store, nn, TYPE_INT);
    }
    TX_COMMIT
      
      return result;
}

int set_readval_add(intset_t *set, int val) {
  int result;

  node_t *prev, *next, *validate, *pvalidate;
    nxt_t nextoffs, validateoffs, pvalidateoffs;

    val_t v;
    TX_START
    prev = ND(set->head);
    nextoffs = prev->next;
    next = ND(nextoffs);

    pvalidate = prev;
    pvalidateoffs = validateoffs = nextoffs;

    v = next->val;
    if (v >= val)
        goto done;

    prev = next;
    nextoffs = prev->next;
    next = ND(nextoffs);

    validate = prev;
    validateoffs = nextoffs;

    while (1) {
        v = next->val;
        if (v >= val)
            break;
        prev = next;
        nextoffs = prev->next;
        next = ND(nextoffs);
        if (pvalidate->next != pvalidateoffs) {
            PRINTD("[A1] Validate failed: expected nxt: %d, got %d", pvalidateoffs, pvalidate->next);
            TX_ABORT(READ_AFTER_WRITE);
        }
        pvalidate = validate;
        pvalidateoffs = validateoffs;
        validate = prev;
        validateoffs = nextoffs;
    }
done:
    result = (v != val);
    if (result) {
        TX_LOAD(&pvalidate->next);
        if ((*(nxt_t *) TX_LOAD(&prev->next)) != validateoffs) {
            PRINTD("[A2] Validate failed: expected nxt: %d, got %d", validateoffs, prev->next);
            TX_ABORT(READ_AFTER_WRITE);
        }
        //fixnxt_t nxt = OF(new_node(val, OF(next), transactional));
        PRINTD("Created node %5d. Value: %d", nxt, val);
        //TX_STORE(&prev->next, &nxt, TYPE_UINT);
        if (pvalidate->next != pvalidateoffs) {
            PRINTD("[A3] Validate failed: expected nxt: %d, got %d", pvalidateoffs, pvalidate->next);
            TX_ABORT(READ_AFTER_WRITE);
        }
    }
    TX_COMMIT


  return result;
}


/*
  set remove --------------------------------------------------------------

 */
static int set_seq_remove(intset_t *set, val_t val);
static int set_early_remove(intset_t *set, val_t val);
static int set_readval_remove(intset_t *set, val_t val);

int set_remove(intset_t *set, val_t val, int transactional) {
    int result = 0;

#ifdef DEBUG
    printf("++> set_remove(%d)\n", (int) val);
    FLUSH;
#endif

#ifdef SEQUENTIAL /* Unprotected */
    return set_seq_remove(set, val);
#endif
#ifdef EARLY_RELEASE
    return set_early_remove(set, val);
#endif
#ifdef READ_VALIDATION
    return set_readval_remove(set, val);
#endif


    node_t prev, next;
    val_t v;

    TX_START
      nxt_t to_store = set->head + sizeof(int);
    TX_LOAD_NODE_NXT(prev, set->head);
    TX_LOAD_NODE(next, prev.next);

    v = next.val;
    if (v >= val) {
      goto done;
    }

    to_store = prev.next + sizeof(int);
    prev.next = next.next;
    TX_LOAD_NODE(next, prev.next);

    while (1) {
      v = next.val;
      if (v >= val) {
	break;
      }

      to_store = prev.next + sizeof(int);
      prev.next = next.next;
      LOAD_NODE(next, prev.next);
    }

 done:
    result = (v == val);
    if (result) {
      TX_STORE(to_store, next.next, TYPE_INT);
      TX_SHFREE((t_vcharp) prev.next);
      //      PRINT("Freed node   %5d. Value: %d", I(prev.next), next.val);
    }

    TX_COMMIT
      return result;
}

static int set_seq_remove(intset_t *set, val_t val) {
  int result;

  node_t prev, next;

#ifdef LOCKS
  global_lock();
#endif
  nxt_t to_store = set->head + sizeof(int);
  LOAD_NODE_NXT(prev, set->head);
  LOAD_NODE(next, prev.next);
  while (next.val < val) {
    to_store = prev.next + sizeof(int);
    prev.next = next.next;
    LOAD_NODE(next, prev.next);
  }
  result = (next.val == val);
  if (result) {
    NONTX_STORE(to_store, prev.next, TYPE_INT);
    sys_shfree((t_vcharp) prev.next);
  }

#ifdef LOCKS
  global_lock_release();
#endif

  return result;
}

static int set_early_remove(intset_t *set, val_t val) {
  int result;

  node_t prev, next;
#ifdef EARLY_RELEASE
  node_t *prls, *pprls;
#endif
  val_t v;

  TX_START
    nxt_t to_store = set->head + sizeof(int);
  TX_LOAD_NODE_NXT(prev, set->head);
  TX_LOAD_NODE(next, prev.next);

  v = next.val;
  if (v >= val) {
    goto done;
  }

#ifdef EARLY_RELEASE
  pprls = prev;
#endif
  to_store = prev.next + sizeof(int);
  prev.next = next.next;
  TX_LOAD_NODE(next, prev.next);

#ifdef EARLY_RELEASE
  prls = prev;
#endif

  while (1) {
    v = next.val;
    if (v >= val) {
      break;
    }

    to_store = prev.next + sizeof(int);
    prev.next = next.next;
    LOAD_NODE(next, prev.next);
#ifdef EARLY_RELEASE
    TX_RRLS(&pprls->next);
    pprls = prls;
    prls = prev;
#endif
  }

 done:
  result = (v == val);
  if (result) {
    TX_STORE(to_store, next.next, TYPE_INT);
    TX_SHFREE((t_vcharp) prev.next);
    //      PRINT("Freed node   %5d. Value: %d", I(prev.next), next.val);
  }

  TX_COMMIT

    return result;
}

static int set_readval_remove(intset_t *set, val_t val) {
  int result;

  node_t *prev, *next, *validate, *pvalidate;
  nxt_t nextoffs, validateoffs, pvalidateoffs;
  val_t v;

  TX_START
    prev = ND(set->head);
  nextoffs = prev->next;
  next = ND(nextoffs);

  pvalidate = prev;
  pvalidateoffs = validateoffs = nextoffs;

  v = next->val;
  if (v >= val)
    goto done;

  prev = next;
  nextoffs = prev->next;
  next = ND(nextoffs);

  validate = prev;
  validateoffs = nextoffs;

  while (1) {
    v = next->val;
    if (v >= val)
      break;
    prev = next;
    nextoffs = prev->next;
    next = ND(nextoffs);
    if (pvalidate->next != pvalidateoffs) {
      PRINTD("[R1] Validate failed: expected nxt: %d, got %d", pvalidateoffs, pvalidate->next);
      TX_ABORT(READ_AFTER_WRITE);
    }
    pvalidate = validate;
    pvalidateoffs = validateoffs;
    validate = prev;
    validateoffs = nextoffs;
  }
 done:
  result = (v == val);
  if (result) {
    TX_LOAD(&pvalidate->next);
    if ((*(nxt_t *) TX_LOAD(&prev->next)) != validateoffs) {
      PRINTD("[R2] Validate failed: expected nxt: %d, got %d", validateoffs, prev->next);
      TX_ABORT(READ_AFTER_WRITE);
    }
    nxt_t *nxt = (nxt_t *) TX_LOAD(&next->next);
    TX_STORE(&prev->next, nxt, TYPE_UINT);
    TX_SHFREE(next);
    if (pvalidate->next != pvalidateoffs) {
      PRINTD("[R3] Validate failed: expected nxt: %d, got %d", pvalidateoffs, pvalidate->next);
      TX_ABORT(READ_AFTER_WRITE);
    }
  }
  TX_COMMIT

  return result;
}

/*
  set print ----------------------------------------------------------------------

 */

void set_print(intset_t* set) {

  printf("min -> ");

  node_t node;
  LOAD_NODE_NXT(node, set->head);
  LOAD_NODE(node, node.next);
    if (node.nextp == NULL) {
        goto null;
    }
    while (node.nextp != NULL) {
        printf("%d -> ", node.val);
	LOAD_NODE(node, node.next);
    }

null:
    PRINTSF("max -> NULL\n");

}
