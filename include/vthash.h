/*
 * File:   rwhash.h
 * Author: trigonak
 *
 * Created on April 19, 2011, 3:04 PM
 * 
 * Data structures and operations to be used for the DS locking
 */

#ifndef VTHASH_H
#define	VTHASH_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "common.h"
#include "rw_entry.h"

#define NO_WRITERI 0x10000000
#define NO_WRITER 0x1000
#define NUM_OF_BUCKETS 48

/* 
 * an entry in a bucket of the hashtable, containing the writer of the address
 * and the readers'  list
 */
typedef struct vthash_bucket_entry {
    uintptr_t address;
    rw_entry_t *rw_entry;
    struct vthash_bucket_entry *next;
} vthash_bucket_entry_t;

/*
 * a bucket in the hashtable containing the bucket_entries
 */
typedef struct vthash_bucket {
    unsigned int nb_entries;
    struct vthash_bucket_entry *head;
} vthash_bucket_t;

/*___________________________________________________________________________________________________
 * Functions
 *___________________________________________________________________________________________________
 */

/*
 * create, initialize and return a bucket_entry
 */
INLINED vthash_bucket_entry_t* vthash_bucket_entry_new(uintptr_t address, vthash_bucket_entry_t * next);

/*  insert a reader or a writer for the bucket_entry->address address to the bucket_entry.
 *  A bucket_entry keeps information about the readers and writer of the bucket_entry->address.
 *
 * Detect possible READ/WRITE, WRITE/READ, and WRITE/WRITE conflicts.
 */
INLINED CONFLICT_TYPE vthash_insert_bucket_entry(vthash_bucket_entry_t *_bucket_entry, nodeid_t nodeId, RW rw);

/*  insert a reader or writer for the address in the bucket. A bucket is a linked list of
 * bucket_entry that hold the metadata for addresses that hash to the same bucket
 */
INLINED CONFLICT_TYPE vthash_insert_bucket(vthash_bucket_t *bucket, nodeid_t nodeId, uintptr_t address, RW rw);

/*  delete a reader or a writer for the bucket_entry->address address from the bucket_entry.
*/
INLINED void vthash_delete_bucket_entry(vthash_bucket_entry_t *bucket_entry, nodeid_t nodeId, RW rw);

/*  delete a reader or writer from the address in the bucket.
*/
INLINED void vthash_delete_bucket(vthash_bucket_t *bucket, nodeid_t nodeId, uintptr_t address, RW rw);


INLINED vthash_bucket_entry_t* 
vthash_bucket_entry_new(uintptr_t address, vthash_bucket_entry_t * next) 
{
    vthash_bucket_entry_t *bucket_entry = (vthash_bucket_entry_t *) malloc(sizeof (vthash_bucket_entry_t));
    if (bucket_entry == NULL) {
        PRINTD("malloc bucket_entry @ bucket_entry_new");
        return NULL;
    }
    bucket_entry->address = address;
    bucket_entry->rw_entry = NULL;
    bucket_entry->next = next;

    return bucket_entry;
}

INLINED CONFLICT_TYPE 
vthash_insert_bucket_entry(vthash_bucket_entry_t *bucket_entry, nodeid_t nodeId, RW rw) 
{
    /*
       TODO: does it make sense to check for "dupicate" entries, when
     * for example a WRITER tries to READ LOCK? Implemented, should I remove it
     * for performance?
     */
    CONFLICT_TYPE conflict = NO_CONFLICT;

    rw_entry_t *rw_entry = bucket_entry->rw_entry;

    if (rw_entry == NULL) {
        rw_entry = rw_entry_new();
        if (rw == WRITE) {
            rw_entry_set_writer(rw_entry, nodeId);
        }
        else {
            rw_entry_set(rw_entry, nodeId);
        }
        bucket_entry->rw_entry = rw_entry;
        return conflict;
    }

	conflict = rw_entry_is_conflicting(rw_entry, nodeId, rw);

	if (conflict == NO_CONFLICT) {
		if (rw == WRITE) {
            rw_entry_set_writer(rw_entry, nodeId);
		} else if (rw == READ) {
            rw_entry_set(rw_entry, nodeId);
		}
	}

	return conflict;
}

INLINED CONFLICT_TYPE
vthash_insert_bucket(vthash_bucket_t *bucket, nodeid_t nodeId, uintptr_t address, RW rw) 
{
	CONFLICT_TYPE conflict = NO_CONFLICT;
    if (bucket->head == NULL) {
        vthash_bucket_entry_t *bucket_entry = vthash_bucket_entry_new(address, NULL);

        conflict = vthash_insert_bucket_entry(bucket_entry, nodeId, rw);

        bucket->head = bucket_entry;
        bucket->nb_entries++;
    }
    else {
        /*need to find the correct existing bucket_entry, or the correct location to insert
         a new bucket_entry*/
        vthash_bucket_entry_t *current = bucket->head;
        vthash_bucket_entry_t *previous = current;
        for (; current != NULL && current->address < address;
                previous = current, current = current->next);

        if (current != NULL && current->address == address) {
            conflict = vthash_insert_bucket_entry(current, nodeId, rw);
        }
        else {
            vthash_bucket_entry_t *bucket_entry = vthash_bucket_entry_new(address, current);
            conflict = vthash_insert_bucket_entry(bucket_entry, nodeId, rw);
            if (current == bucket->head) {
                bucket->head = bucket_entry;
            }
            else {
                previous->next = bucket_entry;
            }
            bucket->nb_entries++;
        }
    }
    return conflict;
}

INLINED void 
vthash_delete_bucket_entry(vthash_bucket_entry_t *bucket_entry, nodeid_t nodeId, RW rw) 
{
    if (bucket_entry->rw_entry == NULL) {
        return;
    }

    if (rw == WRITE) {
        if (rw_entry_is_writer(bucket_entry->rw_entry, nodeId)) {
            rw_entry_unset_writer(bucket_entry->rw_entry);
        }
    }
    else { //rw == READ
        rw_entry_unset(bucket_entry->rw_entry, nodeId);
    }
}

INLINED void 
vthash_delete_bucket(vthash_bucket_t *bucket, nodeid_t nodeId, uintptr_t address, RW rw) 
{
    if (bucket->head != NULL) {
        vthash_bucket_entry_t *current = bucket->head;
        vthash_bucket_entry_t *previous = current;
        while (current != NULL && current->address <= address) {
            if (current->address == address) {
                vthash_delete_bucket_entry(current, nodeId, rw);
                break;
            }
            previous = current;
            current = current->next;
        }

        /*if the bucket is empty, remove it*/
        if (current != NULL && rw_entry_is_empty(current->rw_entry)) {
            if (bucket->head->address == current->address) {
                bucket->head = current->next;
            }
            else {
                previous->next = current->next;
            }

            free(current->rw_entry);
            free(current);
            bucket->nb_entries--;
        }
    }
}

#ifdef	__cplusplus
}
#endif

#endif	/* VTHASH_H */
