/*
 * File:   ps_hashtable.h
 *
 * Data structures and operations to be used for the DS locking
 * The code is used only by the server (DistributedLock manager)
 * Hence, all addresses are of type tm_intern_addr_t
 */

#ifndef PS_HASHTABLE_H
#define PS_HASHTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#include <assert.h>
#include "hash.h"
#include "common.h"
#include "rw_entry.h"

/*
 * This section defines the interface the hash table needs to provide.
 * In the section after this one, we provide the implementation for different
 * types of hashtables.
 */

/*
 * ps_hashtable_t denotes the type of the hashtable. It should be customized
 * to each type of hashtable we use.
 *
 * USE_HASHTABLE_KHASH:  khash.h from <http://www.freewebs.com/attractivechaos/khash.h>
 * USE_HASHTABLE_UTHASH: uthash.h from <>
 * USE_HASHTABLE_SDD:    Sunrise Data Dictionary <>
 * USE_HASHTABLE_VT:     hashtable by Vasilis
 */
    
#ifdef USE_HASHTABLE_KHASH
#include "khash.h"

// name, key_t, val_t, is_map, _hashf, _hasheq
KHASH_MAP_INIT_INT(rw_entry_address,rw_entry_t*);
typedef khash_t(rw_entry_address)* ps_hashtable_t;

#elif  USE_HASHTABLE_UTHASH
#include "uthash.h"

struct uthash_elem_struct {
    tm_intern_addr_t address;            /* we'll use this field as the key */
    rw_entry_t* rw_entry;
    UT_hash_handle hh; /* makes this structure hashable */
};

/*
 * uthash requires that the pointer is initially NULL
 * Hence, ps_hashtable_t will be a pointer to a pointer. This way, we can
 * initialize a variable.
 */
  typedef struct uthash_elem_struct** ps_hashtable_t;

#elif USE_ARRAY
#include "array_log.h"
  //#define NUM_OF_ELEMENTS 1024 * 1024 * 1024 / NUM_DSL_NODES
#define NUM_OF_ELEMENTS 16777216
typedef rw_entry_t* ps_hashtable_t;

#elif  USE_HASHTABLE_SDD
#elif  USE_FIXED_HASH

#include "lock_log.h"
#include "fixed_hash.h"
  typedef fixed_hash_entry_t** ps_hashtable_t;

#elif USE_HASHTABLE_SSHT

#include "ssht.h"
#include "ssht_log.h"
  typedef ssht_hashtable_t ps_hashtable_t;

#elif USE_SSLARRAY

#include "sslarray.h"
#include "sslarray_log.h"
  typedef sslarray_t ps_hashtable_t;

#elif  USE_HASHTABLE_VT
#include "vthash.h"
  typedef vthash_bucket_t** ps_hashtable_t;

#else
#error "No type of hashtable implementation given."
#endif

/*
 * create, initialize and return a ps_hashtable
 */
EXINLINED ps_hashtable_t ps_hashtable_new();

/*
 * insert a reader of writer for the address into the hashatable. The hashtable is the constract that keeps
 * all the metadata for the addresses that the node is responsible.
 *
 * Returns: type of TM conflict caused by inserting given values
 */
EXINLINED CONFLICT_TYPE ps_hashtable_insert(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw);

/*
 * delete a reader of writer for the address from the hashatable.
 */
EXINLINED void ps_hashtable_delete(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw);

EXINLINED void ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, nodeid_t nodeId);

/*
 * traverse and print the ps_hastable contents
 */
EXINLINED void ps_hashtable_print(ps_hashtable_t ps_hashtable);

#ifdef    __cplusplus
}
#endif

#endif    /* PS_HASHTABLE_H */
