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
#define NUM_OF_ELEMENTS 128 * 1024 * 1024
typedef rw_entry_t* ps_hashtable_t;

#elif  USE_HASHTABLE_SDD
#elif  USE_FIXED_HASH

#include "lock_log.h"
#include "fixed_hash.h"
typedef fixed_hash_entry_t** ps_hashtable_t;

#elif  USE_HASHTABLE_VT
#include "vthash.h"
typedef vthash_bucket_t** ps_hashtable_t;

#else
#error "No type of hashtable implementation given."
#endif

/*
 * create, initialize and return a ps_hashtable
 */
INLINED ps_hashtable_t ps_hashtable_new();

/*
 * insert a reader of writer for the address into the hashatable. The hashtable is the constract that keeps
 * all the metadata for the addresses that the node is responsible.
 *
 * Returns: type of TM conflict caused by inserting given values
 */
INLINED CONFLICT_TYPE ps_hashtable_insert(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw);

/*
 * delete a reader of writer for the address from the hashatable.
 */
INLINED void ps_hashtable_delete(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw);

INLINED void ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, nodeid_t nodeId);

/*
 * traverse and print the ps_hastable contents
 */
INLINED void ps_hashtable_print(ps_hashtable_t ps_hashtable);


/*
 * ===========================================================================
 * ===================== The implementation part =============================
 * ===========================================================================
 */

#ifdef USE_HASHTABLE_KHASH

INLINED ps_hashtable_t
ps_hashtable_new()
{
	ps_hashtable_t hash = kh_init(rw_entry_address);
	return hash;
}

INLINED CONFLICT_TYPE
ps_hashtable_insert(ps_hashtable_t ps_hashtable, nodeid_t nodeId, tm_intern_addr_t address, RW rw)
{
	khiter_t k;

	k = kh_get(rw_entry_address, ps_hashtable, address);
	if (k != kh_end(ps_hashtable)) { // the entry exists
		rw_entry_t* rw_entry = kh_value(ps_hashtable, k);
		CONFLICT_TYPE conflict = rw_entry_is_conflicting(rw_entry, nodeId, rw);
		if (conflict != NO_CONFLICT) {
			return conflict;
		} else {
			if (rw == WRITE) {
	rw_entry_set_writer(rw_entry, nodeId);
			} else if (rw == READ) {
	rw_entry_set(rw_entry, nodeId);
			}
		}
	} else { // the entry does not exist, create a new one
	    rw_entry_t* rw_entry = rw_entry_new();
        if (rw == WRITE) {
            rw_entry_set_writer(rw_entry, nodeId);
        }
        else {
            rw_entry_set(rw_entry, nodeId);
        }
        int ret;
        k = kh_put(rw_entry_address, ps_hashtable, address, &ret);
        kh_value(ps_hashtable, k) = rw_entry;
	}
	return NO_CONFLICT;
}

INLINED void
ps_hashtable_delete(ps_hashtable_t ps_hashtable, nodeid_t nodeId, tm_intern_addr_t address, RW rw)
{
	khiter_t k;

	k = kh_get(rw_entry_address, ps_hashtable, address);
    rw_entry_t* entry = kh_value(ps_hashtable, k);

    if (rw == WRITE) {
        if (rw_entry_is_writer(entry, nodeId)) {
            rw_entry_unset_writer(entry);
        }
    }
    else { //rw == READ
        rw_entry_unset(entry, nodeId);
    }
    if (rw_entry_is_empty(entry)) {
		kh_del(rw_entry_address, ps_hashtable, k);
	}
}

INLINED void
ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, nodeid_t nodeId)
{
	khiter_t k;
	for (k = kh_begin(ps_hashtable); k != kh_end(ps_hashtable); ++k)
		if (kh_exist(ps_hashtable, k)) {
			rw_entry_t* entry = kh_value(ps_hashtable, k);

            if (rw_entry_is_writer(entry, nodeId)) {
                rw_entry_unset_writer(entry);
            }

            rw_entry_unset(entry, nodeId);
            if (rw_entry_is_empty(entry)) {
				kh_del(rw_entry_address, ps_hashtable, k);
            }
		}
}

INLINED void
ps_hashtable_print(ps_hashtable_t ps_hashtable)
{
    PRINTS("__PRINT PS_HASHTABLE________________________________________________\n");
	khiter_t k;
	for (k = kh_begin(ps_hashtable); k != kh_end(ps_hashtable); ++k)
		if (kh_exist(ps_hashtable, k)) {
			rw_entry_t* entry = kh_value(ps_hashtable, k);
            PRINTS(" [%-3d]: Write: %-3d\n   ",
		kh_key(ps_hashtable, k), entry->shorts[3]);

            rw_entry_print_readers(entry);
		}
}


#elif  USE_HASHTABLE_UTHASH
INLINED ps_hashtable_t
ps_hashtable_new()
{
    ps_hashtable_t ht = (ps_hashtable_t) malloc(sizeof(struct uthash_elem_struct*));
    if (ht == NULL)
	return NULL;

	*ht = NULL;
	return ht;
}

INLINED CONFLICT_TYPE
ps_hashtable_insert(ps_hashtable_t ps_hashtable, nodeid_t nodeId, tm_intern_addr_t address, RW rw)
{
	struct uthash_elem_struct *el;

	HASH_FIND_INT(*ps_hashtable, &address, el);
	if (el != NULL) { // the entry exists
		rw_entry_t* rw_entry = el->rw_entry;
		CONFLICT_TYPE conflict = rw_entry_is_conflicting(rw_entry, nodeId, rw);
		if (conflict != NO_CONFLICT) {
			return conflict;
		} else {
			if (rw == WRITE) {
				rw_entry_set_writer(rw_entry, nodeId);
			} else if (rw == READ) {
				rw_entry_set(rw_entry, nodeId);
			}
		}
	} else { // the entry does not exist, create a new one
	    rw_entry_t* rw_entry = rw_entry_new();
		el = (struct uthash_elem_struct *)malloc(sizeof(struct uthash_elem_struct));

		if (el == NULL) {
			// XXX: what to return in this case???
			exit(1);
		}

        if (rw == WRITE) {
            rw_entry_set_writer(rw_entry, nodeId);
        }
        else {
            rw_entry_set(rw_entry, nodeId);
        }

		el->address = address;
		el->rw_entry = rw_entry;
		HASH_ADD_INT((*ps_hashtable), address, el);
	}
	return NO_CONFLICT;
}

INLINED void
ps_hashtable_delete(ps_hashtable_t ps_hashtable, nodeid_t nodeId, tm_intern_addr_t address, RW rw)
{
	struct uthash_elem_struct *el;

	HASH_FIND_INT((*ps_hashtable), &address, el);
	if (el != NULL) { // the entry exists
    	rw_entry_t* entry = el->rw_entry;

    	if (rw == WRITE) {
        	if (rw_entry_is_writer(entry, nodeId)) {
            	rw_entry_unset_writer(entry);
        	}
    	}
    	else { //rw == READ
        	rw_entry_unset(entry, nodeId);
    	}

    	if (rw_entry_is_empty(entry)) {
			HASH_DEL((*ps_hashtable), el);
			free(el);
		}
	}
}

INLINED void
ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, nodeid_t nodeId)
{
	struct uthash_elem_struct *el, *tmp;

	HASH_ITER(hh, (*ps_hashtable), el, tmp) {
		rw_entry_t* entry = el->rw_entry;

        if (rw_entry_is_writer(entry, nodeId)) {
            rw_entry_unset_writer(entry);
        }

        rw_entry_unset(entry, nodeId);
        if (rw_entry_is_empty(entry)) {
			HASH_DEL((*ps_hashtable), el);
			free(el);
        }
	}
}

INLINED void
ps_hashtable_print(ps_hashtable_t ps_hashtable)
{
    PRINTS("__PRINT PS_HASHTABLE________________________________________________\n");

	struct uthash_elem_struct *el;

	for (el = (*ps_hashtable); el != NULL; el = el->hh.next) {
		rw_entry_t* entry = el->rw_entry;
		PRINTS(" [%-3d]: Write: %-3d\n   ", el->address, entry->shorts[3]);

		rw_entry_print_readers(entry);
	}
}

#elif USE_ARRAY

INLINED ps_hashtable_t
ps_hashtable_new() {
   ps_hashtable_t ps_hashtable;
   if ((ps_hashtable = (ps_hashtable_t) calloc(NUM_OF_ELEMENTS, sizeof(rw_entry_t)))==NULL) {
      PRINTDNN("calloc ps_hashtable @ os_hashtable_init\n");
      return NULL;
   }

   uintptr_t i;
   for (i = 0; i < NUM_OF_ELEMENTS; i++) {
      ps_hashtable[i].shorts[3] = NO_WRITER;
   }

   return ps_hashtable;
}

INLINED CONFLICT_TYPE
ps_hashtable_insert(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw)
{

   fprintf(stderr, "inserting %d in  %u \n",nodeId, address);
   return rw_entry_is_conflicting(&ps_hashtable[address%NUM_OF_ELEMENTS], nodeId, rw);
}

INLINED void
ps_hashtable_delete(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw)
{
   fprintf(stderr, "deleting %d from %u \n",nodeId, address);
   uintptr_t index = address%NUM_OF_ELEMENTS;
   if (rw == WRITE) {
      if (rw_entry_is_writer(&ps_hashtable[index],nodeId)) {
         rw_entry_unset_writer(&ps_hashtable[index]);
      }
   } else {
      rw_entry_unset(&ps_hashtable[index],nodeId);
   }
}
INLINED void
ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, nodeid_t nodeId)
{

   fprintf(stderr, "deleting node  %d  \n",nodeId);
   uintptr_t i;
   for (i=0;i<NUM_OF_ELEMENTS;i++) {
      if (rw_entry_is_writer(&ps_hashtable[i],nodeId)) {
         rw_entry_unset_writer(&ps_hashtable[i]);
      }
      rw_entry_unset(&ps_hashtable[i],nodeId);
   }
}

INLINED void
ps_hashtable_print(ps_hashtable_t ps_hashtable)
{

    PRINTS("__PRINT PS_HASHTABLE________________________________________________\n");
    uintptr_t i;
    for (i = 0; i < NUM_OF_ELEMENTS; i++) {
        PRINTS(" [%"PRIxIA"]: Write: %-3d\n   ", i, ps_hashtable[i].shorts[3]);
        rw_entry_print_readers(&ps_hashtable[i]);
    }
    FLUSH;
}

#elif  USE_HASHTABLE_SDD

#elif USE_FIXED_HASH

lock_log_set_t** the_logs;
int next_log_free;
int* log_map;

INLINED unsigned int ps_get_hash(uintptr_t address){
    return hash_tw((address>>2) % UINT_MAX);
}


INLINED ps_hashtable_t
ps_hashtable_new()
{
    int i;
    the_logs=(lock_log_set_t**) malloc(NUM_APP_NODES*sizeof(lock_log_set_t*));
    log_map=(int*)malloc(NUM_UES * sizeof(int));
    for (i=0;i<NUM_APP_NODES;i++){
        the_logs[i]=lock_log_set_new();
    }
    next_log_free=0;
    for (i=0;i<NUM_UES;i++) {
        log_map[i]=-1;
    }
    return fixed_hash_init(); 
}
INLINED CONFLICT_TYPE
ps_hashtable_insert(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw)
{
    //fprintf(stderr, "start insert %u\n",address);
    //usleep(100);
    if (log_map[nodeId]==-1) {
        log_map[nodeId]=next_log_free;
        next_log_free++;
    }
    CONFLICT_TYPE conflict = fixed_hash_insert_in_bucket(ps_hashtable, ps_get_hash(address)%NUM_OF_BUCKETS, nodeId, address, rw, the_logs[log_map[nodeId]]);
    //fprintf(stderr, "end insert\n");
    //if (conflict!=NO_CONFLICT)fprintf(stderr, "with conflict!\n");
    //usleep(100);
    return conflict;
}

INLINED void
ps_hashtable_delete(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw)
{
    //fprintf(stderr, "start delete\n");
    //usleep(100);
    fixed_hash_delete_from_bucket(ps_hashtable, ps_get_hash(address)%NUM_OF_BUCKETS, nodeId, address, rw);
    //fprintf(stderr, "end delete\n");
    //usleep(100);
}

INLINED void
ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, nodeid_t nodeId)
{
    //fprintf(stderr, "start del all\n");
    //usleep(100);
    lock_log_set_t* the_log = the_logs[log_map[nodeId]];
    int i;
    for (i=0;i<the_log->nb_entries;i++) {
       if (the_log->lock_log_entries[i].rw % 2 == 1) {
        fixed_hash_delete_from_entry((fixed_hash_entry_t*)the_log->lock_log_entries[i].address, the_log->lock_log_entries[i].index, nodeId, READ);
       }
        if (the_log->lock_log_entries[i].rw >= 2) {
         fixed_hash_delete_from_entry((fixed_hash_entry_t*)the_log->lock_log_entries[i].address, the_log->lock_log_entries[i].index, nodeId, WRITE);
       }
    }
    lock_log_set_empty(the_log);
    //fprintf(stderr, "end del all\n");
    //usleep(100);
}

INLINED void
ps_hashtable_print(ps_hashtable_t ps_hashtable)
{
}

#elif  USE_HASHTABLE_VT

INLINED unsigned int ps_get_hash(uintptr_t address){
    return hash_tw((address>>2) % UINT_MAX);
}

#ifdef DEBUG_UTILIZATION
    extern int bucket_usages[];
    extern int bucket_current[];
    extern int bucket_max[];
#endif

INLINED ps_hashtable_t
ps_hashtable_new()
{
    ps_hashtable_t ps_hashtable;

    if ((ps_hashtable = (ps_hashtable_t) malloc(NUM_OF_BUCKETS * sizeof (vthash_bucket_t *))) == NULL) {
        PRINTDNN("malloc ps_hashtable @ ps_hashtable_init\n");
        return NULL;
    }

    int i;
    for (i = 0; i < NUM_OF_BUCKETS; i++) {
        ps_hashtable[i] = (vthash_bucket_t *) malloc(sizeof (vthash_bucket_t));
        if (ps_hashtable[i] == NULL) {
            while (--i >= 0) {
                free(ps_hashtable[i]);
            }
            free(ps_hashtable);
            PRINTDNN("malloc ps_hashtable[i] @ ps_hashtable_init\n");
            return NULL;
        }
        ps_hashtable[i]->nb_entries = 0;
        ps_hashtable[i]->head = NULL;
    }

    return ps_hashtable;
}

INLINED CONFLICT_TYPE
ps_hashtable_insert(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw)
{
#ifdef DEBUG_UTILIZATION
    unsigned int index = ps_get_hash(address)%NUM_OF_BUCKETS;
    bucket_usages[index]++;
    if (bucket_max[index]<ps_hashtable[index]->nb_entries) {
        bucket_max[index]=ps_hashtable[index]->nb_entries;
    }
#endif
    return vthash_insert_bucket(ps_hashtable[ps_get_hash(address) % NUM_OF_BUCKETS], nodeId, address, rw);
}

INLINED void
ps_hashtable_delete(ps_hashtable_t ps_hashtable, nodeid_t nodeId,
                                tm_intern_addr_t address, RW rw)
{
#ifdef DEBUG_UTILIZATION
    bucket_current[ps_get_hash(address)%NUM_OF_BUCKETS]--;
#endif
    vthash_delete_bucket(ps_hashtable[ps_get_hash(address) % NUM_OF_BUCKETS], nodeId, address, rw);
}

INLINED void
ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, nodeid_t nodeId)
{
    int nbucket;
    for (nbucket = 0; nbucket < NUM_OF_BUCKETS; nbucket++) {
        vthash_bucket_t *bucket = ps_hashtable[nbucket];
        if (bucket->head != NULL) {
            vthash_bucket_entry_t *be_current = bucket->head;
            vthash_bucket_entry_t *be_previous = be_current;
            while (be_current != NULL) {

                //todo: do i need to check for null?? be_current->rw_entry

                if (rw_entry_is_writer(be_current->rw_entry, nodeId)) {
                    rw_entry_unset_writer(be_current->rw_entry);
                }

                rw_entry_unset(be_current->rw_entry, nodeId);

                if (rw_entry_is_empty(be_current->rw_entry)) {
                    if (bucket->head->address == be_current->address) {
                        bucket->head = be_current->next;
                    }
                    else {
                        be_previous->next = be_current->next;
                    }

                    free(be_current->rw_entry);
                    free(be_current);
                    bucket->nb_entries--;
                }
                else {
                    be_previous = be_current;
                }

                be_current = be_current->next;
            }

        }
    }
}

/*
 * traverse and print the ps_hastable contents
 */
INLINED void
ps_hashtable_print(ps_hashtable_t ps_hashtable)
{

    PRINTS("__PRINT PS_HASHTABLE________________________________________________\n");
    int i;
    for (i = 0; i < NUM_OF_BUCKETS; i++) {
        vthash_bucket_t *bucket = ps_hashtable[i];
        PRINTS("Bucket %-3d | #Entries: %-3d\n", i, bucket->nb_entries);
        vthash_bucket_entry_t *curbe = bucket->head;
        while (curbe != NULL) {
            rw_entry_t *rwe = curbe->rw_entry;
            PRINTS(" [%"PRIxIA"]: Write: %-3d\n   ", curbe->address, rwe->shorts[3]);
            rw_entry_print_readers(rwe);


            curbe = curbe->next;
        }
    }
    FLUSH;
}

#endif

#ifdef    __cplusplus
}
#endif

#endif    /* PS_HASHTABLE_H */
