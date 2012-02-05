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

#define SINGLEBITS_

    CONFLICT_TYPE ps_conflict_type;

    /*__________________________________________________________________________________________________
     * RW ENTRY                                                                                         |
     * _________________________________________________________________________________________________|
     */

#define NO_WRITERI 0x10000000
#define NO_WRITER 0x1000
#define NUM_OF_BUCKETS 48


#define FIELD(rw_entry, field) (rw_entry->n##field)
#define FIELDA(rw_entry, field) (rw_entry.n##field)

    typedef struct rw_entry {

        union {
#ifdef SINGLEBITS
            struct {
                unsigned int n0 : 1;
                unsigned int n1 : 1;
                unsigned int n2 : 1;
                unsigned int n3 : 1;
                unsigned int n4 : 1;
                unsigned int n5 : 1;
                unsigned int n6 : 1;
                unsigned int n7 : 1;
                unsigned int n8 : 1;
                unsigned int n9 : 1;
                unsigned int n10 : 1;
                unsigned int n11 : 1;
                unsigned int n12 : 1;
                unsigned int n13 : 1;
                unsigned int n14 : 1;
                unsigned int n15 : 1;
                unsigned int n16 : 1;
                unsigned int n17 : 1;
                unsigned int n18 : 1;
                unsigned int n19 : 1;
                unsigned int n20 : 1;
                unsigned int n21 : 1;
                unsigned int n22 : 1;
                unsigned int n23 : 1;
                unsigned int n24 : 1;
                unsigned int n25 : 1;
                unsigned int n26 : 1;
                unsigned int n27 : 1;
                unsigned int n28 : 1;
                unsigned int n29 : 1;
                unsigned int n30 : 1;
                unsigned int n31 : 1;
                unsigned int n32 : 1;
                unsigned int n33 : 1;
                unsigned int n34 : 1;
                unsigned int n35 : 1;
                unsigned int n36 : 1;
                unsigned int n37 : 1;
                unsigned int n38 : 1;
                unsigned int n39 : 1;
                unsigned int n40 : 1;
                unsigned int n41 : 1;
                unsigned int n42 : 1;
                unsigned int n43 : 1;
                unsigned int n44 : 1;
                unsigned int n45 : 1;
                unsigned int n46 : 1;
                unsigned int n47 : 1;
                unsigned short int writer;
            } bits;
#endif
            unsigned short int shorts[4];
            unsigned int ints[2];
        };

    } rw_entry_t;

    /* an entry in a bucket of the hashtable, containing the writer of the address
     * and the readers'  list
     */
    typedef struct bucket_entry {
        unsigned int address;
        rw_entry_t *rw_entry;
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

    inline BOOLEAN rw_entry_member(rw_entry_t *rwe, unsigned short nodeId);

    inline void rw_entry_set(rw_entry_t *rwe, unsigned short nodeId);

    inline void rw_entry_unset(rw_entry_t *rwe, unsigned short nodeId);

    inline void rw_entry_empty(rw_entry_t *rwe);

    inline BOOLEAN rw_entry_is_empty(rw_entry_t *rwe);

    inline BOOLEAN rw_entry_is_unique_reader(rw_entry_t *rwe, unsigned int nodeId);

    inline void rw_entry_set_writer(rw_entry_t *rwe, unsigned short nodeId);

    inline void rw_entry_unset_writer(rw_entry_t *rwe);

    inline BOOLEAN rw_entry_has_writer(rw_entry_t *rwe);

    inline BOOLEAN rw_entry_is_writer(rw_entry_t *rwe, unsigned short nodeId);

    inline rw_entry_t * rw_entry_new();

    /*  create, initialize and return a bucket_entry
     */
    inline bucket_entry_t * bucket_entry_new(int address, bucket_entry_t * next);

    inline void rw_entry_print_readers(rw_entry_t *rwe);

    /*  insert a reader or a writer for the bucket_entry->address address to the bucket_entry.
     *  A bucket_entry keeps information about the readers and writer of the bucket_entry->address.
     *
     * Detect possible READ/WRITE, WRITE/READ, and WRITE/WRITE conflicts and call the
     * Contention Manager.
     */
    inline void rw_entry_insert_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw);

    /*  insert a reader or writer for the address in the bucket. A bucket is a linked list of
     * bucket_entry that hold the metadata for addresses that hash to the same bucket
     */
    inline void rw_entry_insert_bucket(bucket_t *bucket, int nodeId, int address, RW rw);

    /*  create, initialize and return a ps_hashtable
     */
    extern ps_hashtable_t ps_hashtable_new();

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


#ifdef	__cplusplus
}
#endif

#endif	/* RWHASH_H */
