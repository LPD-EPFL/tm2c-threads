/*
 * File:   rwhash.c
 * Author: trigonak
 * 
 * Created on June 01, 2011, 6:10PM
 */

#include "rwhash.h"

inline BOOLEAN rw_entry_member(rw_entry_t *rwe, unsigned short nodeId) {
    return (BOOLEAN) ((rwe->ints[(int) (nodeId / 32)] >> (nodeId % 32)) & 0x01);
}

inline void rw_entry_set(rw_entry_t *rwe, unsigned short nodeId) {
    rwe->ints[(int) (nodeId / 32)] |= (1 << (nodeId % 32));
}

inline void rw_entry_unset(rw_entry_t *rwe, unsigned short nodeId) {
    rwe->ints[(int) (nodeId / 32)] &= (~(1 << (nodeId % 32)));
}

inline void rw_entry_empty(rw_entry_t *rwe) {
    rwe->ints[0] = 0;
    rwe->shorts[3] = NO_WRITER;
}

inline BOOLEAN rw_entry_is_empty(rw_entry_t *rwe) {
    return (BOOLEAN) (rwe->ints[0] == 0 && rwe->ints[1] == NO_WRITERI);
}

inline BOOLEAN rw_entry_is_unique_reader(rw_entry_t *rwe, unsigned int nodeId) {

    union {
        unsigned long long int lli;
        unsigned int i[2];
        unsigned short s[4];
    } convert;
    convert.lli = 0;
    convert.i[(int) (nodeId / 32)] = (1 << (nodeId % 32));
    return (BOOLEAN) (convert.i[0] == rwe->ints[0] && convert.s[2] == rwe->shorts[2]);
}

inline void rw_entry_set_writer(rw_entry_t *rwe, unsigned short nodeId) {
    rwe->shorts[3] = nodeId;
}

inline void rw_entry_unset_writer(rw_entry_t *rwe) {
    rwe->shorts[3] = NO_WRITER;
}

inline BOOLEAN rw_entry_has_writer(rw_entry_t *rwe) {
    return (BOOLEAN) (rwe->shorts[3] != NO_WRITER);
}

inline BOOLEAN rw_entry_is_writer(rw_entry_t *rwe, unsigned short nodeId) {
    return (BOOLEAN) (rwe->shorts[3] == nodeId);
}

inline rw_entry_t * rw_entry_new() {
    rw_entry_t *r = (rw_entry_t *) calloc(1, sizeof (rw_entry_t));
    if (r == NULL) {
        PRINTD("malloc r @ rw_entry_new");
        return NULL;
    }
    r->shorts[3] = NO_WRITER;
    return r;
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
    bucket_entry->rw_entry = NULL;
    bucket_entry->next = next;

    return bucket_entry;
}

inline void rw_entry_print_readers(rw_entry_t *rwe) {

    union {
        unsigned long long int lli;
        unsigned int i[2];
    } convert;

    convert.i[0] = rwe->ints[0];
    convert.i[1] = rwe->shorts[2];
    int i;
    for (i = 0; i < 48; i++) {
        if (convert.lli & 0x01) {
            PRINTF("%d -> ", i);
        }
        convert.lli >>= 1;
    }

    PRINTF("NULL\n");
    FLUSHD;
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
    ps_conflict_type = NO_CONFLICT;

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
        return;
    }

    if (rw == WRITE) { /*publishing*/
        if (rw_entry_has_writer(rw_entry)) { /*WRITE/WRITE conflict*/
            //TODO: here the logic for WRITE -> WRITE
            PRINTD("[X] %d tries to write %d: WRITE lock by %d", nodeId, bucket_entry->address, rw_entry->shorts[3]);

            /*change it with CM*/
            ps_conflict_type = WRITE_AFTER_WRITE;
        }
        else if (!rw_entry_is_empty(rw_entry)) { /*Possible READ/WRITE*/
            /*if the only writer is the one that "asks"*/
            if (rw_entry_is_unique_reader(rw_entry, nodeId)) {
                rw_entry_set_writer(rw_entry, nodeId);
            }
            else { /*READ/WRITE conflict*/
                //TODO: here the logic for READ -> WRITE
                ME;
                PRINTF("[X] %d tries to write %d: READ lock by ", nodeId, bucket_entry->address);
                rw_entry_print_readers(rw_entry);

                /*change it with CM*/
                ps_conflict_type = WRITE_AFTER_READ;
            }
        }
        else {
            rw_entry_set_writer(rw_entry, nodeId);
        }
    }
    else { /*subscribing*/
        if (rw_entry_has_writer(rw_entry) && !rw_entry_is_writer(rw_entry, nodeId)) { /*WRITE/READ*/
            //TODO: here the logic for WRITE -> READ
            PRINTD("[X] %d tries to read %d: WRITE lock by %d", nodeId, bucket_entry->address, rw_entry->shorts[3]);

            /*change it with CM*/
            ps_conflict_type = READ_AFTER_WRITE;
        }
        else {
            rw_entry_set(rw_entry, nodeId);
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
    }
    else {
        /*need to find the correct existing bucket_entry, or the correct location to insert
         a new bucket_entry*/
        bucket_entry_t *current = bucket->head;
        bucket_entry_t *previous = current;
        for (; current != NULL && current->address < address;
                previous = current, current = current->next);

        if (current != NULL && current->address == address) {
            rw_entry_insert_bucket_entry(current, nodeId, rw);
        }
        else {
            bucket_entry_t *bucket_entry = bucket_entry_new(address, current);
            rw_entry_insert_bucket_entry(bucket_entry, nodeId, rw);
            if (current == bucket->head) {
                bucket->head = bucket_entry;
            }
            else {
                previous->next = bucket_entry;
            }
            bucket->nb_entries++;
        }
    }
}

/*  create, initialize and return a ps_hashtable
 */
inline ps_hashtable_t ps_hashtable_new() {
    ps_hashtable_t ps_hashtable;

    if ((ps_hashtable = (ps_hashtable_t) malloc(NUM_OF_BUCKETS * sizeof (bucket_t *))) == NULL) {
        PRINTDNN("malloc ps_hashtable @ ps_hashtable_init\n");
        return NULL;
    }

    int i;
    for (i = 0; i < NUM_OF_BUCKETS; i++) {
        ps_hashtable[i] = (bucket_t *) malloc(sizeof (bucket_t));
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

/* insert a reader of writer for the address into the hashatable. The hashtable is the constract that keeps
 * all the metadata for the addresses that the node is responsible.
 */
inline void ps_hashtable_insert(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw) {

    rw_entry_insert_bucket(ps_hashtable[address % NUM_OF_BUCKETS], nodeId, address, rw);
}

/*  delete a reader or a writer for the bucket_entry->address address from the bucket_entry.
 */
inline void rw_entry_delete_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw) {
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

/* delete a reader of writer for the address from the hashatable.
 */
inline void ps_hashtable_delete(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw) {
    rw_entry_delete_bucket(ps_hashtable[address % NUM_OF_BUCKETS], nodeId, address, rw);
}

inline void ps_hashtable_delete_node(ps_hashtable_t ps_hashtable, unsigned int nodeId) {
    int nbucket;
    for (nbucket = 0; nbucket < NUM_OF_BUCKETS; nbucket++) {
        bucket_t *bucket = ps_hashtable[nbucket];
        if (bucket->head != NULL) {
            bucket_entry_t *be_current = bucket->head;
            bucket_entry_t *be_previous = be_current;
            while (be_current != NULL) {

                //TODO: do i need to check for null?? be_current->rw_entry

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

/*  traverse and print the ps_hastable contents
 */
inline void ps_hashtable_print(ps_hashtable_t ps_hashtable) {

    PRINTF("__PRINT PS_HASHTABLE________________________________________________\n");
    int i;
    for (i = 0; i < NUM_OF_BUCKETS; i++) {
        bucket_t *bucket = ps_hashtable[i];
        PRINTF("Bucket %-3d | #Entries: %-3d\n", i, bucket->nb_entries);
        bucket_entry_t *curbe = bucket->head;
        while (curbe != NULL) {
            rw_entry_t *rwe = curbe->rw_entry;
            PRINTF(" [%-3d]: Write: %-3d\n   ", curbe->address, rwe->shorts[3]);
            rw_entry_print_readers(rwe);


            curbe = curbe->next;
        }
    }
    FLUSHD;
}
