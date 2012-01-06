/*
 * File:   rwhash.c
 * Author: trigonak
 * 
 * Created on June 01, 2011, 6:10PM
 */

#include "rwhash.h"

#ifdef DEBUG_UTILIZATION
unsigned int bucket_usages[NUM_OF_BUCKETS];
#endif

#define USE_MACROS

#ifdef USE_MACROS
#ifndef BITOPTS_
#define rw_entry_member(be, nodeId)     \
        ((BOOLEAN) ((be->rw_entry.ints[(int) (nodeId / 32)] >> (nodeId % 32)) & 0x01))
#else
#define rw_entry_member(be, nodeId)      \
        test_bit(nodeId, (unsigned long *) be->rw_entry.ints);
#endif

#else

inline BOOLEAN rw_entry_member(bucket_entry_t *be, unsigned short nodeId) {
    return (BOOLEAN) ((be->rw_entry.ints[(int) (nodeId / 32)] >> (nodeId % 32)) & 0x01);
}
#endif

#ifdef USE_MACROS
#ifndef BITOPTS
#define rw_entry_set(be, nodeId)        \
        be->rw_entry.ints[(int) (nodeId / 32)] |= (1 << (nodeId % 32))
#else
#define rw_entry_set(be, nodeId)        \
        set_bit(nodeId, (unsigned long *) be->rw_entry.ints)
        //set_bit(nodeId % 32, (unsigned long *) &be->rw_entry.ints[(int) nodeId / 32])
#endif
#else

inline void rw_entry_set(bucket_entry_t *be, unsigned short nodeId) {
    be->rw_entry.ints[(int) (nodeId / 32)] |= (1 << (nodeId % 32));
}
#endif

#ifdef USE_MACROS
#ifndef BITOPTS
#define rw_entry_unset(be, nodeId)      \
        be->rw_entry.ints[(int) (nodeId / 32)] &= (~(1 << (nodeId % 32)))
#else
#define rw_entry_unset(be, nodeId)      \
        clear_bit(nodeId, (unsigned long *) be->rw_entry.ints)
        //clear_bit(nodeId % 32, (unsigned long *) &be->rw_entry.ints[(int) nodeId / 32])
#endif
#else

inline void rw_entry_unset(bucket_entry_t *be, unsigned short nodeId) {
    be->rw_entry.ints[(int) (nodeId / 32)] &= (~(1 << (nodeId % 32)));
}
#endif

#ifdef USE_MACROS
#define rw_entry_empty(be)              \
        be->rw_entry.ints[0] = 0;       \
        be->rw_entry.shorts[3] = NO_WRITER;
#else
inline void rw_entry_empty(bucket_entry_t *be) {
    be->rw_entry.ints[0] = 0;
    be->rw_entry.shorts[3] = NO_WRITER;
}
#endif

#ifdef USE_MACROS
#define rw_entry_is_empty(be)           \
        ((BOOLEAN) (be->rw_entry.ints[0] == 0 && be->rw_entry.ints[1] == NO_WRITERI))
#else
inline BOOLEAN rw_entry_is_empty(bucket_entry_t *be) {
    return (BOOLEAN) (be->rw_entry.ints[0] == 0 && be->rw_entry.ints[1] == NO_WRITERI);
}
#endif

inline BOOLEAN rw_entry_is_unique_reader(bucket_entry_t *be, unsigned int nodeId) {

    union {
        unsigned long long int lli;
        unsigned int i[2];
        unsigned short s[4];
    } convert;
    convert.lli = 0;
    convert.i[(int) (nodeId / 32)] = (1 << (nodeId % 32));
    return (BOOLEAN) (convert.i[0] == be->rw_entry.ints[0] && convert.s[2] == be->rw_entry.shorts[2]);
}

#ifdef USE_MACROS
#define rw_entry_set_writer(be, nodeId)         \
    be->rw_entry.shorts[3] = nodeId;
#else
inline void rw_entry_set_writer(bucket_entry_t *be, unsigned short nodeId) {
    be->rw_entry.shorts[3] = nodeId;
}
#endif

#ifdef USE_MACROS
#define rw_entry_unset_writer(be)               \
    be->rw_entry.shorts[3] = NO_WRITER;
#else
inline void rw_entry_unset_writer(bucket_entry_t *be) {
    be->rw_entry.shorts[3] = NO_WRITER;
}
#endif

#ifdef USE_MACROS
#define rw_entry_has_writer(be)                 \
        ((BOOLEAN) (be->rw_entry.shorts[3] != NO_WRITER))
#else
inline BOOLEAN rw_entry_has_writer(bucket_entry_t *be) {
    return (BOOLEAN) (be->rw_entry.shorts[3] != NO_WRITER);
}
#endif

#ifdef USE_MACROS
#define rw_entry_is_writer(be, nodeId)          \
        ((BOOLEAN) (be->rw_entry.shorts[3] == nodeId))
#else
inline BOOLEAN rw_entry_is_writer(bucket_entry_t *be, unsigned short nodeId) {
    return (BOOLEAN) (be->rw_entry.shorts[3] == nodeId);
}
#endif

/*  create, initialize and return a bucket_entry
 */
inline bucket_entry_t * bucket_entry_new(int address, bucket_entry_t * next) {
    bucket_entry_t *bucket_entry = (bucket_entry_t *) malloc(sizeof (bucket_entry_t));
    if (bucket_entry == NULL) {
        PRINTD("malloc bucket_entry @ bucket_entry_new");
        return NULL;
    }
    bucket_entry->address = address;
    bucket_entry->rw_entry.ll = 0;
    bucket_entry->rw_entry.shorts[3] = NO_WRITER;
    bucket_entry->next = next;

    return bucket_entry;
}

inline void rw_entry_print_readers(bucket_entry_t *be) {

    union {
        unsigned long long int lli;
        unsigned int i[2];
    } convert;

    convert.i[0] = be->rw_entry.ints[0];
    convert.i[1] = be->rw_entry.shorts[2];
    int i;
    for (i = 0; i < NUM_UES; i++) {
        if (convert.lli & 0x01) {
            PRINTS("%d -> ", i);
        }
        convert.lli >>= 1;
    }

    PRINTSF("NULL\n");
}

/*  insert a reader or a writer for the bucket_entry->address address to the bucket_entry.
 *  A bucket_entry keeps information about the readers and writer of the bucket_entry->address.
 *
 * Detect possible READ/WRITE, WRITE/READ, and WRITE/WRITE conflicts and call the
 * Contention Manager.
 */
void rw_entry_insert_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw) {
    /*
       TODO: does it make sense to check for "dupicate" entries, when
     * for example a WRITER tries to READ LOCK? Implemented, should I remove it
     * for performance?
     */
    ps_conflict_type = NO_CONFLICT;


    if (bucket_entry->rw_entry.ints[0] == 0 && bucket_entry->rw_entry.ints[1] == NO_WRITERI) {
        if (rw == WRITE) {
            rw_entry_set_writer(bucket_entry, nodeId);
        }
        else {
            rw_entry_set(bucket_entry, nodeId);
        }
        return;
    }

    if (rw == WRITE) { /*publishing*/
        if (rw_entry_has_writer(bucket_entry)) { /*WRITE/WRITE conflict*/
            //TODO: here the logic for WRITE -> WRITE
            PRINTD("[X] %d tries to write %d: WRITE lock by %d", nodeId, bucket_entry->address, bucket_entry->rw_entry.shorts[3]);

            /*change it with CM*/
            ps_conflict_type = WRITE_AFTER_WRITE;
        }
        else if (!rw_entry_is_empty(bucket_entry)) { /*Possible READ/WRITE*/
            /*if the only writer is the one that "asks"*/
            if (rw_entry_is_unique_reader(bucket_entry, nodeId)) {
                rw_entry_set_writer(bucket_entry, nodeId);
            }
            else { /*READ/WRITE conflict*/
                //TODO: here the logic for READ -> WRITE
                ME;
                PRINTF("[X] %d tries to write %d: READ lock by ", nodeId, bucket_entry->address);
#ifdef DEBUG
                rw_entry_print_readers(rw_entry);
#endif

                /*change it with CM*/
                ps_conflict_type = WRITE_AFTER_READ;
            }
        }
        else {
            rw_entry_set_writer(bucket_entry, nodeId);
        }
    }
    else { /*subscribing*/
        if (rw_entry_has_writer(bucket_entry) && !rw_entry_is_writer(bucket_entry, nodeId)) { /*WRITE/READ*/
            //TODO: here the logic for WRITE -> READ
            PRINTD("[X] %d tries to read %d: WRITE lock by %d", nodeId, bucket_entry->address, bucket_entry->rw_entry.shorts[3]);

            /*change it with CM*/
            ps_conflict_type = READ_AFTER_WRITE;
        }
        else {
            rw_entry_set(bucket_entry, nodeId);
        }
    }
}

/*  insert a reader or writer for the address in the bucket. A bucket is a linked list of
 * bucket_entry that hold the metadata for addresses that hash to the same bucket
 */
void rw_entry_insert_bucket(bucket_t *bucket, int nodeId, int address, RW rw) {

    if (bucket->head == NULL) {
#ifndef MEM_PREALLOC
        bucket_entry_t *bucket_entry = bucket_entry_new(address, NULL);

#else
        bucket_entry_t *bucket_entry = mem_prealloc_alloc(address, NULL);
#endif
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
#ifndef MEM_PREALLOC
            bucket_entry_t *bucket_entry = bucket_entry_new(address, current);

#else
            bucket_entry_t *bucket_entry = mem_prealloc_alloc(address, current);
#endif

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

#ifdef MEM_PREALLOC
    mem_prealloc_init();
#endif

    return ps_hashtable;
}

/*destroy a hashtable*/
inline void ps_hashtable_destroy(ps_hashtable_t ht) {
    int i;
    for (i = 0; i < NUM_OF_BUCKETS; i++) {
        bucket_t *bucket = ht[i];
        bucket_entry_t *be = bucket->head, *freed;
        while (be != NULL) {
            freed = be;
            be = be->next;
#ifndef MEM_PREALLOC
            free(freed);
#else
            mem_prealloc_free(freed);
#endif
        }

        free(bucket);
    }

#ifdef MEM_PREALLOC
    mem_prealloc_destroy();
#endif

    free(ht);
}

/* insert a reader of writer for the address into the hashatable. The hashtable is the constract that keeps
 * all the metadata for the addresses that the node is responsible.
 */
inline void ps_hashtable_insert(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw) {
    unsigned int h = HASH(address);
    //unsigned int h = (address);

#ifdef DEBUG_UTILIZATION
    bucket_usages[h % NUM_OF_BUCKETS]++;
#endif
    
    rw_entry_insert_bucket(ps_hashtable[h % NUM_OF_BUCKETS], nodeId, address, rw);
}

/*  delete a reader or a writer for the bucket_entry->address address from the bucket_entry.
 */
inline void rw_entry_delete_bucket_entry(bucket_entry_t *bucket_entry, int nodeId, RW rw) {
/*
    if (bucket_entry->rw_entry == NULL) {
        return;
    }
*/

    if (rw == WRITE) {
        if (rw_entry_is_writer(bucket_entry, nodeId)) {
            rw_entry_unset_writer(bucket_entry);
        }
    }
    else { //rw == READ
        rw_entry_unset(bucket_entry, nodeId);
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
        if (current != NULL && rw_entry_is_empty(current)) {
            if (bucket->head->address == current->address) {
                bucket->head = current->next;
            }
            else {
                previous->next = current->next;
            }

#ifndef MEM_PREALLOC
            free(current);
#else
            mem_prealloc_free(current);
#endif

            bucket->nb_entries--;
        }
    }
}

/* delete a reader of writer for the address from the hashatable.
 */
inline void ps_hashtable_delete(ps_hashtable_t ps_hashtable, unsigned int nodeId, unsigned int address, RW rw) {
    
    
    rw_entry_delete_bucket(ps_hashtable[HASH(address) % NUM_OF_BUCKETS], nodeId, address, rw);
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

                if (rw_entry_is_writer(be_current, nodeId)) {
                    rw_entry_unset_writer(be_current);
                }

                rw_entry_unset(be_current, nodeId);

                if (rw_entry_is_empty(be_current)) {
                    if (bucket->head->address == be_current->address) {
                        bucket->head = be_current->next;
                    }
                    else {
                        be_previous->next = be_current->next;
                    }

#ifndef MEM_PREALLOC
                    free(be_current);
#else
                    mem_prealloc_free(be_current);
#endif

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

    PRINTS("__PRINT PS_HASHTABLE________________________________________________\n");
    int i;
    for (i = 0; i < NUM_OF_BUCKETS; i++) {
        bucket_t *bucket = ps_hashtable[i];
        PRINTS("Bucket %-3d | #Entries: %-3d\n", i, bucket->nb_entries);
        bucket_entry_t *curbe = bucket->head;
        while (curbe != NULL) {
            PRINTS(" [%-3d]: Write: %-3d\n   ", curbe->address, curbe->rw_entry.shorts[3]);
            rw_entry_print_readers(curbe);


            curbe = curbe->next;
        }
    }
    FLUSH;
}

#ifdef MEM_PREALLOC
mem_prealloc_t mprealloc__;

void mem_prealloc_init() {
    mprealloc__.entries = (bucket_entry_t **) malloc(PREALLOC_BUCKETS * sizeof (bucket_entry_t *));
    if (mprealloc__.entries == NULL) {
        PRINT("malloc @ mem_prealloc_init");
        EXIT(-1);
    }

    int i;
    for (i = 0; i < PREALLOC_BUCKETS; i++) {
        mprealloc__.entries[i] = (bucket_entry_t *) malloc(sizeof (bucket_entry_t));
        if (mprealloc__.entries[i] == NULL) {
            PRINT("malloc @ mem_prealloc_init");
            EXIT(-1);
        }
        mprealloc__.entries[i]->rw_entry.ints[0] = 0;
        mprealloc__.entries[i]->rw_entry.ints[1] = NO_WRITERI;
        mprealloc__.entries[i]->next = NULL;
    }

    mprealloc__.index = 0;

    PRINTD("initialized %d bucket_entry_t", PREALLOC_BUCKETS);
}

/*      free the resources used by the preallocator
 */

void mem_prealloc_destroy() {
    int i;
    for (i = 0; i < PREALLOC_BUCKETS; i++) {
        free(mprealloc__.entries[i]);
    }
    free(mprealloc__.entries);

    PRINTD("freed %d bucket_entry_t", PREALLOC_BUCKETS);
}

/*      allocate bucket_entry_t through the preallocator
 */
inline bucket_entry_t * mem_prealloc_alloc(unsigned int address, bucket_entry_t * next) {
    if (mprealloc__.index < PREALLOC_BUCKETS) {
        mprealloc__.entries[mprealloc__.index]->address = address;
        mprealloc__.entries[mprealloc__.index]->next = next;
        PRINTD("Allocated %2d -- %p | Free: %d", mprealloc__.index, mprealloc__.entries[mprealloc__.index], PREALLOC_BUCKETS - mprealloc__.index - 1);
        return mprealloc__.entries[mprealloc__.index++];
        }
    PRINTD("Allocated with MALLOC --");
    return bucket_entry_new(address, next);
            }

/*      free bucket_entry_t using preallocation
 */
inline void mem_prealloc_free(bucket_entry_t * be) {
    if (mprealloc__.index == 0) {
        PRINTD("Freed %p, with FREE", be);
        free(be);
        return;
    }

    mprealloc__.entries[--(mprealloc__.index)] = be;

    be->rw_entry.ints[0] = 0;
    be->rw_entry.ints[1] = NO_WRITERI;

    PRINTD("Freed %p, placed @ %d", be, mprealloc__.index);

}

#endif
