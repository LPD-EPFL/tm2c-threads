#ifndef FIXED_HASH
#define FIXED_HASH

#ifdef	__cplusplus
extern "C" {
#endif

#include "common.h"
#include "rw_entry.h"
#include "lock_log.h"

#define NO_WRITERI 0x10000000
#define NO_WRITER 0x1000
#define NUM_OF_BUCKETS 47

#define NUM_SLOTS_PER_ENTRY 8
//could be 16 for 32-bit architectures

unsigned short isEmpty[NUM_OF_BUCKETS];

typedef struct fixed_hash_entry {
    //first cache line: 7 addresses plus a pointer to the next entry
    //TODO what if 32-bit architecture?
    uintptr_t addresses[NUM_SLOTS_PER_ENTRY-1];
    struct fixed_hash_entry *next;
    //second cache line: 8 rw_entries (currently, sizeof(rw_entry) is 8)
    rw_entry_t rw_entries[NUM_SLOTS_PER_ENTRY];
} fixed_hash_entry_t;

typedef struct fixed_hash_bucket {
    struct fixed_hash_entry *head;
    //unsigned int nb_entries; //not really needed; enable for debugging?
} fixed_hash_bucket_t;

//create new hash entry and return a pointer to it
INLINED fixed_hash_entry_t* new_hash_entry(){
    fixed_hash_entry_t * ret = (fixed_hash_entry_t *) malloc(sizeof(fixed_hash_entry_t));
    //fix all the addresses to NULL, meaning no valid rw_entry for that slot
    int i;
    for (i =0; i<NUM_SLOTS_PER_ENTRY-1; i++) {
        ret->addresses[i]=0;
    }
    ret->next=NULL;
    for (i =0; i<NUM_SLOTS_PER_ENTRY; i++) {
        ret->rw_entries[i].ints[0]=0;
        ret->rw_entries[i].ints[1]=NO_WRITERI;
    }

    return ret;
}

//allocate memory and initlialize the hash
INLINED fixed_hash_entry_t** fixed_hash_init(){
    fixed_hash_entry_t** the_hash = (fixed_hash_entry_t**) malloc(NUM_OF_BUCKETS * sizeof(fixed_hash_entry_t *));
    int i;
    for (i=0;i<NUM_OF_BUCKETS;i++) {
          the_hash[i]=new_hash_entry();
          isEmpty[i]=1;
    }
    return the_hash;
}


INLINED CONFLICT_TYPE fixed_hash_insert_in_bucket(fixed_hash_entry_t** the_hash, int bucket_id, nodeid_t nodeId, uintptr_t address, RW rw, lock_log_set_t* the_log){
    //first, check if address is present
    //if yes, do op
    //if not, find the first free slot, and do a the op on that rw_entry; if no free slot, create a new hash entry
    //fprintf(stderr, "ins %u in bucket %d\n",address,bucket_id);
    //usleep(100);
    unsigned short i=0;
    fixed_hash_entry_t* current=the_hash[bucket_id];
    rw_entry_t* rw_entry = NULL;
    if (isEmpty[bucket_id]==0)
    while (i<(NUM_SLOTS_PER_ENTRY-1)){
        //fprintf(stderr, "i is %d\n",i);
        //usleep(100);
        if (current->addresses[i]==address) {
            rw_entry=&(current->rw_entries[i]);
            break;
        }
        if (i==(NUM_SLOTS_PER_ENTRY-2)){
            if (current->next!=NULL) {
               //fprintf(stderr, "Max but not null\n");
               current=current->next;
               i=0;
               continue;
            }
        }
        i++;
    }
    //fprintf(stderr, "ch1\n");
    //usleep(100);
    if (rw_entry==NULL) {
        //fprintf(stderr, "In 2\n");
        i=0;
        current=the_hash[bucket_id];
        while (i<(NUM_SLOTS_PER_ENTRY-1)){
          if (current->addresses[i]==0) {
                current->addresses[i]=address;
                rw_entry=&(current->rw_entries[i]);
                break;
           }
           if (i==(NUM_SLOTS_PER_ENTRY-2)){
                if (current->next!=NULL) {
                   current=current->next;
                   i=0;
                  continue;
                }
           }
        i++;
       }
       if (rw_entry==NULL) {
            //fprintf(stderr, "Creating new entry\n");
            current->next=new_hash_entry();
            current=current->next;
            current->addresses[0]=address;
            i=0;
            rw_entry=&(current->rw_entries[0]);
       }
    }
    
	CONFLICT_TYPE conflict = NO_CONFLICT;
    conflict = rw_entry_is_conflicting(rw_entry, nodeId, rw);

     if (conflict == NO_CONFLICT) {
         if (rw == WRITE) {
             rw_entry_set_writer(rw_entry, nodeId);
         } else if (rw == READ) {
             rw_entry_set(rw_entry, nodeId);
         }
     }

    if (conflict==NO_CONFLICT){
        unsigned short logRW = (rw==READ) ? 1 : 2;
        lock_log_set_insert(the_log,(uintptr_t) current, i, logRW);
    }
    //fprintf(stderr, "finised insert in bucket\n");
    //usleep(100);
    isEmpty[bucket_id]=0;
     return conflict;
   
}

INLINED void fixed_hash_delete_from_entry(fixed_hash_entry_t* theEntry,int index, nodeid_t nodeId, RW rw){
    if (theEntry->addresses[index]==0) {
        return;
    }

    rw_entry_t* rw_entry = &(theEntry->rw_entries[index]);

    if (rw == WRITE) {
        if (rw_entry_is_writer(rw_entry, nodeId)) {
            rw_entry_unset_writer(rw_entry);
        }
    } else {
        rw_entry_unset(rw_entry,nodeId);
    }

    //check if empty; if yes, set address to NULL to mark an unused slot
    if ((rw_entry->ints[1]==NO_WRITERI) && (rw_entry->ints[0]==0)){
        theEntry->addresses[index]=0;  
    } 

}

INLINED void fixed_hash_delete_from_bucket(fixed_hash_entry_t** the_hash, int bucket_id, nodeid_t nodeId,  uintptr_t address, RW rw){
    int i=0;
    fixed_hash_entry_t* current=the_hash[bucket_id];
    rw_entry_t* rw_entry=NULL;
    while (i<(NUM_SLOTS_PER_ENTRY-1)){
       if (current->addresses[i]==address){
            fixed_hash_delete_from_entry(current, i, nodeId, rw);
            break;
       }
       if (i==(NUM_SLOTS_PER_ENTRY-2)){
        if (current->next!=NULL) {
            current=current->next;
            i=0;
            continue;
        }
       }
       i++;
    }
}


#ifdef	__cplusplus
}
#endif
#endif
