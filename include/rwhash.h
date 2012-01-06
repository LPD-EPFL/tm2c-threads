/*
 * File:   rwhash.h
 * Author: trigonak
 *
 * Created on April 19, 2011, 3:04 PM
 * 
 * Data structures and operations to be used for the DS locking
 */

#ifndef RWHASH_H
#define	RWHASH_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "common.h"


#define MEM_PREALLOC
#define USE_MACROS_

#define BITOPTS_
    
#ifdef BITOPTS
#include "bitopts.h"
#endif

    CONFLICT_TYPE ps_conflict_type;

    /*__________________________________________________________________________________________________
     * RW ENTRY                                                                                         |
     * _________________________________________________________________________________________________|
     */

#define NO_WRITERI      0x10000000
#define NO_WRITER       0x1000
#define NUM_OF_BUCKETS  16

//#define HASH(address)   ((((((unsigned int) address)) ^ 2) >> 8))
//#define HASH(address)   ((((((unsigned int) address)))>> 3))
#define HASH(address)   (address)



#ifdef DEBUG_UTILIZATION
    extern unsigned int bucket_usages[NUM_OF_BUCKETS];
#endif

    /* an entry in a bucket of the hashtable, containing the writer of the address
     * and the readers'  list
     */
    typedef struct bucket_entry {
        unsigned int address;

        struct {

        union {
            unsigned short int shorts[4];
            unsigned int ints[2];
                unsigned long long ll;
        };
        } rw_entry;

        struct bucket_entry *next;
    } bucket_entry_t;

    /* a bucket in the hashtable containing the bucket_entries
     */
    typedef struct bucket {
        unsigned int nb_entries;
        struct bucket_entry *head;
    } bucket_t;

    typedef bucket_t **ps_hashtable_t;
    
    
    /*___________________________________________________________________________________________________
     * Functions
     *___________________________________________________________________________________________________
     */



    inline BOOLEAN rw_entry_member(bucket_entry_t *be, unsigned short nodeId);

    inline void rw_entry_set(bucket_entry_t *be, unsigned short nodeId);

    inline void rw_entry_unset(bucket_entry_t *be, unsigned short nodeId);

    inline void rw_entry_empty(bucket_entry_t *be);

    inline BOOLEAN rw_entry_is_empty(bucket_entry_t *be);

    inline BOOLEAN rw_entry_is_unique_reader(bucket_entry_t *be, unsigned int nodeId);

    inline void rw_entry_set_writer(bucket_entry_t *be, unsigned short nodeId);

    inline void rw_entry_unset_writer(bucket_entry_t *be);

    inline BOOLEAN rw_entry_has_writer(bucket_entry_t *be);

    /*  create, initialize and return a bucket_entry
     */
    inline bucket_entry_t * bucket_entry_new(int address, bucket_entry_t * next);

    inline void rw_entry_print_readers(bucket_entry_t *be);

    /*  insert a reader or a writer for the bucket_entry->address address to the bucket_entry.
     *  A bucket_entry keeps information about the readers and writer of the bucket_entry->address.
     *
     * Detect possible READ/WRITE, WRITE/READ, and WRITE/WRITE conflicts and call the
     * Contention Manager.
     */
    void rw_entry_insert_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw);

    /*  insert a reader or writer for the address in the bucket. A bucket is a linked list of
     * bucket_entry that hold the metadata for addresses that hash to the same bucket
     */
    void rw_entry_insert_bucket(bucket_t *bucket, int nodeId, int address, RW rw);

    /*  create, initialize and return a ps_hashtable
     */
    extern ps_hashtable_t ps_hashtable_new();

    /*  destroy a hashtable
     */
    inline void ps_hashtable_destroy(ps_hashtable_t ht);

    /* insert a reader of writer for the address into the hashatable. The hashtable is the constract that keeps
     * all the metadata for the addresses that the node is responsible.
     */
    extern void ps_hashtable_insert(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw);

    /*  delete a reader or a writer for the bucket_entry->address address from the bucket_entry.
     */
    inline void rw_entry_delete_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw);

    /*  delete a reader or writer from the address in the bucket.
     */
    inline void rw_entry_delete_bucket(bucket_t *bucket, int nodeId, int address, RW rw);

    /* delete a reader of writer for the address from the hashatable.
     */
    extern void ps_hashtable_delete(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw);

    extern void ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, unsigned int nodeId);

    /*  traverse and print the ps_hastable contents
     */
    extern void ps_hashtable_print(ps_hashtable_t ps_hashtable);


#ifdef MEM_PREALLOC
#define PREALLOC_BUCKETS        1024
    
    typedef struct {
        bucket_entry_t **entries;
        unsigned int index;
    } mem_prealloc_t;

    void mem_prealloc_init();
    void mem_prealloc_destroy();
    inline bucket_entry_t * mem_prealloc_alloc();
    inline void mem_prealloc_free(bucket_entry_t * be);
#endif


#ifdef	__cplusplus
}
#endif

#endif	/* RWHASH_H */

