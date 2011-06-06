/*
 * File:   hashtable.h
 * Author: trigonak
 *
 * Created on March 28, 2011, 4:36 PM
 * 
 * Data-structure to be used for the DS locking. 
 * DEPRECATED : rwhash is used instead
 */

#ifndef HASHTABLE_H
#define	HASHTABLE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "common.h"

#define NUM_OF_BUCKETS 48 /*num of buckets in the hashtable*/
#define NO_WRITER 49 /*signifies that nobody is publishing*/

    CONFLICT_TYPE ps_conflict_type;

    /*  an entry in the linked-list of the reader for an address
     */
    typedef struct read_entry {
        unsigned int node;
        struct read_entry *next;
    } read_entry_t;

    /*  a linked-list containing the read entries
     */
    typedef struct read_entries {
        unsigned int nb_entries;
        struct read_entry *head;
    } read_entries_t;

    /* an entry in a bucket of the hashtable, containing the writer of the address
     * and the readers' linked list
     */
    typedef struct bucket_entry {
        unsigned int address;
        unsigned int writer;
        read_entries_t *r_entries;
        struct bucket_entry *next;
    } bucket_entry_t;

    /* a bucket in the hashtable containing the bucket_entries
     */
    typedef struct bucket {
        unsigned int nb_entries;
        struct bucket_entry *head;
    } bucket_t;

    /* the hastable type = bucket_t *ps_hashtable_t[NUM_OF_BUCKETS];
     */
    typedef bucket_t **ps_hashtable_t;

    typedef struct {
        double time_since_started;
    } tx_info_t;

    /*__________________________________________________________________________________________________
     * INSERTING                                                                                        |
     * _________________________________________________________________________________________________|
     */

    /*  inserting one more reader (nodeId) to the head of the read_entries list. The read_entries list
     * keeps the reader currently subscribed to an address.
     */
    inline void read_entry_insert_read_entries(read_entries_t *read_entries, int nodeId);

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

    /* insert a reader of writer for the address into the hashatable. The hashtable is the constract that keeps
     * all the metadata for the addresses that the node is responsible.
     */
    inline void ps_hashtable_insert(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw);

    /*__________________________________________________________________________________________________
     * DELETING                                                                                         |
     * _________________________________________________________________________________________________|
     */

    /*  deletes a reader (nodeId) from the read_entries list.
     */
    inline void read_entry_delete_read_entries(read_entries_t *read_entries, int nodeId);

    /*  delete a reader or a writer for the bucket_entry->address address from the bucket_entry.
     */
    inline void rw_entry_delete_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw);

    /*  delete a reader or writer from the address in the bucket.
     */
    inline void rw_entry_delete_bucket(bucket_t *bucket, int nodeId, int address, RW rw);

    /* delete a reader of writer for the address from the hashatable.
     */
    inline void ps_hashtable_delete(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw);

    /*__________________________________________________________________________________________________
     * CREATING                                                                                         |
     * _________________________________________________________________________________________________|
     */

    /*  create, initialize and return a read_entry
     */
    inline read_entry_t * read_entry_new(int nodeId, read_entry_t *next);

    /*  create, initialize and return a bucket_entry
     */
    inline bucket_entry_t * bucket_entry_new(int address, bucket_entry_t *next);

    /*  create, initialize and return a bucket
     */
    inline bucket_t * bucket_new();

    /*  create, initialize and return a ps_hashtable
     */
    inline ps_hashtable_t ps_hashtable_new();

    /*__________________________________________________________________________________________________
     * DEBUGGING                                                                                        |
     * _________________________________________________________________________________________________|
     */

    /*  traverse and print a read_entries list
     */
    inline void read_entries_print(read_entries_t *res);

    /*  print the data from a bucket_entry
     */
    inline void bucket_entry_print(bucket_entry_t *bucket_entry);

    /*  traverse and print the bucket data from a bucket list
     */
    inline void bucket_print(bucket_t *bucket);

    /*  traverse and print the ps_hastable contents
     */
    inline void ps_hashtable_print(ps_hashtable_t ps_hashtable);

    /*__________________________________________________________________________________________________
     * METADATA                                                                                         |
     * _________________________________________________________________________________________________|
     */

    inline void update_tx_info(tx_info_t tx_info[], int nodeId, double time_since_started);

    inline double calc_local_time_started(double now, double time_since_started);


    /*______________________________________________________________________________________________________________________
     * IMPLEMENTATION                                                                                                       |
     * _____________________________________________________________________________________________________________________|
     */



    /*__________________________________________________________________________________________________
     * INSERTING                                                                                        |
     * _________________________________________________________________________________________________|
     */

    /*  inserting one more reader (nodeId) to the head of the read_entries list. The read_entries list
     * keeps the reader currently subscribed to an address.
     */
    inline void read_entry_insert_read_entries(read_entries_t *read_entries, int nodeId) {

        read_entries->nb_entries++;
        read_entry_t *read_entry = read_entry_new(nodeId, read_entries->head);
        read_entries->head = read_entry; //insert the new entry in the head of the list
    }

    /*  insert a reader or a writer for the bucket_entry->address address to the bucket_entry.
     *  A bucket_entry keeps information about the readers and writer of the bucket_entry->address.
     *
     * Detect possible READ/WRITE, WRITE/READ, and WRITE/WRITE conflicts and call the
     * Contention Manager.
     */
    inline void rw_entry_insert_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw) {
        /*
           TODO: does it make sense to check for "dupicate" entries, when
         * for example a WRITER tries to READ LOCK? Implemented, should I remove it
         * for performance?
         */
        read_entries_t *read_entries = bucket_entry->r_entries;

        ps_conflict_type = NO_CONFLICT;

        if (rw == WRITE) { /*publishing*/
            if (bucket_entry->writer != NO_WRITER) { /*WRITE/WRITE conflict*/
                //TODO: here the logic for WRITE -> WRITE
                //printf(">> %d tries to write %d: WRITE lock by %d\n", nodeId, bucket_entry->address, bucket_entry->writer);

                /*change it with CM*/
                ps_conflict_type = WRITE_AFTER_WRITE;
            } else if (read_entries != NULL && read_entries->nb_entries != 0) { /*Possible READ/WRITE*/
                /*if the only writer is the one that "asks"*/
                if (read_entries->nb_entries == 1 && read_entries->head->node == nodeId) {
                    bucket_entry->writer = nodeId;
                } else { /*READ/WRITE conflict*/
                    //TODO: here the logic for READ -> WRITE
                    //printf(">> %d tries to write %d: READ lock by ", nodeId, bucket_entry->address);
                    //read_entries_print(read_entries);

                    /*change it with CM*/
                    ps_conflict_type = WRITE_AFTER_READ;
                }
            } else {
                bucket_entry->writer = nodeId;
            }
        } else { /*subscribing*/
            if (bucket_entry->writer != NO_WRITER && bucket_entry->writer != nodeId) { /*WRITE/READ*/
                //TODO: here the logic for WRITE -> READ
                //printf(">> %d tries to read %d: WRITE lock by %d\n", nodeId, bucket_entry->address, bucket_entry->writer);

                /*change it with CM*/
                ps_conflict_type = READ_AFTER_WRITE;
            } else {
                if (read_entries == NULL) {
                    read_entries_t *r_entries = (read_entries_t *) malloc(sizeof (read_entries_t));
                    if (r_entries == NULL) {
                        //TODO: return a special ps_conflict_type value??
                        PRINTD("malloc r_entries @ rw_entry_insert_bucket_entry");
                        return;
                    }
                    r_entries->nb_entries = 0;
                    r_entries->head = NULL;
                    read_entry_insert_read_entries(r_entries, nodeId);
                    bucket_entry->r_entries = r_entries;
                } else {
                    read_entry_insert_read_entries(read_entries, nodeId);
                }
            }
        }
    }

    /*  insert a reader or writer for the address in the bucket. A bucket is a linked list of
     * bucket_entry that hold the metadata for addresses that hash to the same bucket
     */
    inline void rw_entry_insert_bucket(bucket_t *bucket, int nodeId, int address, RW rw) {

        if (bucket->head == NULL) {
            bucket_entry_t *bucket_entry = bucket_entry_new(address, NULL);
            rw_entry_insert_bucket_entry(bucket_entry, nodeId, rw);
            bucket->head = bucket_entry;

            bucket->nb_entries++;
        } else {
            /*need to find the correct existing bucket_entry, or the correct location to insert
             a new bucket_entry*/
            bucket_entry_t *current = bucket->head;
            bucket_entry_t *previous = current;
            for (; current != NULL && current->address < address;
                    previous = current, current = current->next);

            if (current != NULL && current->address == address) {
                rw_entry_insert_bucket_entry(current, nodeId, rw);
            } else {
                bucket_entry_t *bucket_entry = bucket_entry_new(address, current);
                rw_entry_insert_bucket_entry(bucket_entry, nodeId, rw);
                if (current == bucket->head) {
                    bucket->head = bucket_entry;
                } else {
                    previous->next = bucket_entry;
                }
                bucket->nb_entries++;
            }
        }
    }

    /* insert a reader of writer for the address into the hashatable. The hashtable is the constract that keeps
     * all the metadata for the addresses that the node is responsible.
     */
    inline void ps_hashtable_insert(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw) {

        bucket_t *bucket = ps_hashtable[address % NUM_OF_BUCKETS];

        rw_entry_insert_bucket(bucket, nodeId, address, rw);
    }

    /*__________________________________________________________________________________________________
     * DELETING                                                                                         |
     * _________________________________________________________________________________________________|
     */

    /*  deletes a reader (nodeId) from the read_entries list.
     */
    inline void read_entry_delete_read_entries(read_entries_t *read_entries, int nodeId) {

        read_entry_t *current = read_entries->head;
        if (current != NULL) {
            if (current->node == nodeId) {
                read_entries->head = current->next;
                free(current);
                read_entries->nb_entries--;
            } else {
                read_entry_t *previous = current;
                while (current != NULL) {
                    if (current->node == nodeId) {
                        previous->next = current->next;
                        free(current);
                        read_entries->nb_entries--;
                        return;
                    }
                    previous = current;
                    current = current->next;
                }
            }
        }
    }

    /*  delete a reader or a writer for the bucket_entry->address address from the bucket_entry.
     */
    inline void rw_entry_delete_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw) {
        if (rw == WRITE) {
            if (bucket_entry->writer == nodeId) {
                bucket_entry->writer = NO_WRITER;
            }
        } else { //rw == READ
            if (bucket_entry->r_entries != NULL) {
                read_entry_delete_read_entries(bucket_entry->r_entries, nodeId);
            }
        }
    }

    /*  delete a reader or writer from the address in the bucket.
     */
    inline void rw_entry_delete_bucket(bucket_t *bucket, int nodeId, int address, RW rw) {
        if (bucket->head != NULL) {
            bucket_entry_t *current = bucket->head;
            bucket_entry_t *previous = current;
            while (current != NULL && current->address <= address) {
                if (current->address == address) {
                    rw_entry_delete_bucket_entry(current, nodeId, rw);
                    break;
                }
                previous = current;
                current = current->next;
            }

            /*if the bucket is empty, remove it*/
            if (current != NULL && current->writer == NO_WRITER &&
                    (current->r_entries == NULL || current->r_entries->nb_entries == 0)) {
                if (bucket->head->address == current->address) {
                    bucket->head = current->next;
                } else {
                    previous->next = current->next;
                }

                free(current->r_entries);
                free(current);
                bucket->nb_entries--;
            }
        }
    }

    /* delete a reader of writer for the address from the hashatable.
     */
    inline void ps_hashtable_delete(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw) {
        bucket_t *bucket = ps_hashtable[address % NUM_OF_BUCKETS];

        rw_entry_delete_bucket(bucket, nodeId, address, rw);
    }

    /*__________________________________________________________________________________________________
     * CREATING                                                                                         |
     * _________________________________________________________________________________________________|
     */

    /*  create, initialize and return a read_entry
     */
    inline read_entry_t * read_entry_new(int nodeId, read_entry_t * next) {
        read_entry_t *read_entry = (read_entry_t *) malloc(sizeof (read_entry_t));
        if (read_entry == NULL) {
            PRINTD("malloc read_entry @ read_entry_new");
            return NULL;
        }
        read_entry->node = nodeId;
        read_entry->next = next;

        return read_entry;
    }

    /*  create, initialize and return a bucket_entry
     */
    inline bucket_entry_t * bucket_entry_new(int address, bucket_entry_t * next) {
        bucket_entry_t *bucket_entry = (bucket_entry_t *) malloc(sizeof (bucket_entry_t));
        if (bucket_entry == NULL) {
            PRINTD("malloc bucket_entry @ bucket_entry_new");
            return NULL;
        }
        bucket_entry->address = address;
        bucket_entry->writer = NO_WRITER;
        bucket_entry->r_entries = NULL;
        bucket_entry->next = next;

        return bucket_entry;
    }

    /*  create, initialize and return a bucket
     */
    inline bucket_t * bucket_new() {
        bucket_t *bucket = (bucket_t *) malloc(sizeof (bucket_t));
        if (bucket == NULL) {
            PRINTD("malloc bucket @ bucket_new");
            return NULL;
        }
        bucket->nb_entries = 0;
        bucket->head = NULL;

        return bucket;
    }

    /*  create, initialize and return a ps_hashtable
     */
    inline ps_hashtable_t ps_hashtable_new() {
        ps_hashtable_t ps_hashtable;

        if ((ps_hashtable = (ps_hashtable_t) malloc(NUM_OF_BUCKETS * sizeof (bucket_t *))) == NULL) {
            printf("malloc ps_hashtable @ ps_hashtable_init\n");
            return NULL;
        }

        int i;
        for (i = 0; i < NUM_OF_BUCKETS; i++) {
            ps_hashtable[i] = bucket_new();
            if (ps_hashtable[i] == NULL) {
                while (--i >= 0) {
                    free(ps_hashtable[i]);
                }
                free(ps_hashtable);
            }
        }

        return ps_hashtable;
    }

    /*__________________________________________________________________________________________________
     * DEBUGGING                                                                                        |
     * _________________________________________________________________________________________________|
     */

    /*  traverse and print a read_entries list
     */
    inline void read_entries_print(read_entries_t * res) {
        read_entry_t *temp = res->head;
        while (temp != NULL) {
            printf("%d -> ", temp->node);
            temp = temp->next;
        }
        printf("NULL\n");
    }

    /*  print the data from a bucket_entry
     */
    inline void bucket_entry_print(bucket_entry_t *bucket_entry) {
        printf("Bucket Entry for Address %-5d\n", bucket_entry->address);
        printf("  Writer: %2d\n  ", bucket_entry->writer);
        read_entries_print(bucket_entry->r_entries);
    }

    /*  traverse and print the bucket data from a bucket list
     */
    inline void bucket_print(bucket_t *bucket) {
        printf("#Entries: %-3d\n", bucket->nb_entries);
        bucket_entry_t *current = bucket->head;
        while (current != NULL) {
            bucket_entry_print(current);
            current = current->next;
        }
    }

    /*  traverse and print the ps_hastable contents
     */
    inline void ps_hashtable_print(ps_hashtable_t ps_hashtable) {
        printf("_____________________________________________________________________________\n");

        int i;
        for (i = 0; i < NUM_OF_BUCKETS; i++) {
            bucket_t *bucket = ps_hashtable[i];
            printf("Bucket %-3d | #Entries: %-3d\n", i, bucket->nb_entries);
            bucket_entry_t *curbe = bucket->head;
            while (curbe != NULL) {
                read_entries_t *res = curbe->r_entries;
                printf(" [%-3d]: Write: %-3d | #Entries: %-3d\n   ", curbe->address, curbe->writer, (res == NULL ? 0 : res->nb_entries));

                if (res != NULL) {
                    read_entry_t *temp = res->head;
                    while (temp != NULL) {
                        printf("%d -> ", temp->node);
                        temp = temp->next;
                    }
                }
                printf("NULL\n");

                curbe = curbe->next;
            }
        }
        fflush(stdout);
    }

    /*__________________________________________________________________________________________________
     * METADATA                                                                                         |
     * _________________________________________________________________________________________________|
     */

    inline void update_tx_info(tx_info_t tx_info[], int nodeId, double time_since_started) {
        tx_info[nodeId].time_since_started = time_since_started;
    }

    inline double calc_local_time_started(double now, double time_since_started) {
        return now - time_since_started;
    }

#ifdef	__cplusplus
}
#endif

#endif	/* HASHTABLE_H */

