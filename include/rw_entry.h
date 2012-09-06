/*
 * File:   rw_entry.h
 *
 * Data structures to be used for the DS locking
 */

#ifndef RW_ENTRY_H
#define	RW_ENTRY_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "common.h"
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
#include "cm.h"
#endif

/*__________________________________________________________________________________________________
* RW ENTRY                                                                                         |
* _________________________________________________________________________________________________|
*/

#define NO_WRITERI 0x10000000
#define NO_WRITERLL 0x1000000000000000
#define NO_WRITER 0x1000
#define NUM_OF_BUCKETS 47 


#define FIELD(rw_entry, field) (rw_entry->n##field)
#define FIELDA(rw_entry, field) (rw_entry.n##field)

typedef struct rw_entry {
  union {
    uint16_t shorts[4];
    uint32_t ints[2];
    uint64_t ll;
  };
} rw_entry_t;

/*___________________________________________________________________________________________________
 * Functions
 *___________________________________________________________________________________________________
 */

INLINED BOOLEAN rw_entry_is_member(rw_entry_t *rwe, nodeid_t nodeId);

INLINED void rw_entry_set(rw_entry_t *rwe, nodeid_t nodeId);

INLINED void rw_entry_unset(rw_entry_t *rwe, nodeid_t nodeId);

INLINED void rw_entry_clear(rw_entry_t *rwe);

INLINED BOOLEAN rw_entry_is_empty(rw_entry_t *rwe);

INLINED BOOLEAN rw_entry_is_unique_reader(rw_entry_t *rwe, nodeid_t nodeId);

INLINED void rw_entry_set_writer(rw_entry_t *rwe, nodeid_t nodeId);

INLINED void rw_entry_unset_writer(rw_entry_t *rwe);

INLINED BOOLEAN rw_entry_has_writer(rw_entry_t *rwe);

INLINED BOOLEAN rw_entry_is_writer(rw_entry_t *rwe, nodeid_t nodeId);

INLINED rw_entry_t* rw_entry_new();

INLINED CONFLICT_TYPE rw_entry_is_conflicting(rw_entry_t* rw_entry, nodeid_t nodeId, RW rw);

INLINED void rw_entry_print_readers(rw_entry_t *rwe);

/*___________________________________________________________________________________________________
 * Implementation
 *___________________________________________________________________________________________________
 */

INLINED BOOLEAN
rw_entry_is_member(rw_entry_t *rwe, nodeid_t nodeId)
{
  //    return (BOOLEAN) ((rwe->ints[(int) (nodeId / 32)] >> (nodeId % 32)) & 0x01);
  return (BOOLEAN) ((rwe->ll >> nodeId) & 0x1);
}

INLINED void
rw_entry_set(rw_entry_t *rwe, nodeid_t nodeId)
{
  //   rwe->ints[(int) (nodeId / 32)] |= (1 << (nodeId % 32));
  rwe->ll |= (1LL << nodeId);
}

INLINED void
rw_entry_unset(rw_entry_t *rwe, nodeid_t nodeId)
{
  //    rwe->ints[(int) (nodeId / 32)] &= (~(1 << (nodeId % 32)));
  rwe->ll &= ~(1LL << nodeId);
}

INLINED void
rw_entry_clear(rw_entry_t *rwe)
{
    /* rwe->ints[0] = 0; */
    /* rwe->ints[1] = NO_WRITERI; */
  rwe->ll = NO_WRITERLL;
}

INLINED BOOLEAN
rw_entry_is_empty(rw_entry_t *rwe)
{
  //   return (BOOLEAN) (rwe->ints[0] == 0 && rwe->ints[1] == NO_WRITERI);
  return (BOOLEAN) (rwe->ll == NO_WRITERLL);
}

INLINED BOOLEAN
rw_entry_is_unique_reader(rw_entry_t *rwe, nodeid_t nodeId)
{

  rw_entry_t tmp;
  tmp.ll = NO_WRITERLL;
  rw_entry_set(&tmp, nodeId);
  return (BOOLEAN) (tmp.ll == rwe->ll);
}

INLINED
void rw_entry_fetch_readers(rw_entry_t *rwe, unsigned short *readers) {

    union {
        unsigned long long int lli;
        unsigned int i[2];
    } convert;

    convert.i[0] = rwe->ints[0];
    convert.i[1] = rwe->shorts[2];
    int i;
    for (i = 0; i < NUM_UES; i++) {
        if (convert.lli & 0x01) {
            readers[i] = 1;
        }
        else {
            readers[i] = 0;
        }
        convert.lli >>= 1;
    }
}

INLINED void
rw_entry_set_writer(rw_entry_t *rwe, nodeid_t nodeId)
{
    rwe->shorts[3] = nodeId;
}

INLINED void
rw_entry_unset_writer(rw_entry_t *rwe)
{
    rwe->shorts[3] = NO_WRITER;
}

INLINED BOOLEAN
rw_entry_has_writer(rw_entry_t *rwe)
{
    return (BOOLEAN) (rwe->shorts[3] != NO_WRITER);
}

INLINED BOOLEAN
rw_entry_is_writer(rw_entry_t *rwe, nodeid_t nodeId)
{
    return (BOOLEAN) (rwe->shorts[3] == nodeId);
}

INLINED rw_entry_t*
rw_entry_new()
{
  rw_entry_t *r = (rw_entry_t *) malloc(sizeof(rw_entry_t));
    /* if (r == NULL) { */
    /*     PRINTD("malloc r @ rw_entry_new"); */
    /*     return NULL; */
    /* } */
  r->ll = NO_WRITERLL;
    return r;
}

    /*
     * Detect whether the new insert into the entry would cause a possible
     * READ/WRITE, WRITE/READ, and WRITE/WRITE conflict.
     */
    INLINED CONFLICT_TYPE
    rw_entry_is_conflicting(rw_entry_t* rw_entry, nodeid_t nodeId, RW rw) {
        //XXX: is it needed??
        //    if (rw_entry == NULL) {
        //        return NO_CONFLICT;
        //    }
        
        if (rw == WRITE) { /*publishing*/
            if (rw_entry_has_writer(rw_entry)) { /*WRITE/WRITE conflict*/
                //here the logic for WRITE -> WRITE
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
                if (contention_manager(nodeId, &rw_entry->shorts[3], WRITE_AFTER_WRITE) == TRUE) {
                    //attacker won - old aborted
                    return NO_CONFLICT;
                } 
                else {
                    return WRITE_AFTER_WRITE;
                }
#else
		return WRITE_AFTER_WRITE;
#endif	/* NOCM */
            } 
            else if (!rw_entry_is_empty(rw_entry)) { /*Possible WRITE AFTER READ*/
                /* /\*if the only writer is the one that "asks"*\/ */
                /* if (rw_entry_is_unique_reader(rw_entry, nodeId)) { */
                /*     return NO_CONFLICT; */
                /* }  */
                /* else {  */
		  /*WRITE AFTER READ conflict*/
                    // here the logic for READ -> WRITE
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
                    unsigned short readers[NUM_UES];
                    rw_entry_fetch_readers(rw_entry, readers);
                    readers[nodeId] = 0;
                    
                    if (contention_manager(nodeId, readers, WRITE_AFTER_READ) == TRUE) {
                        rw_entry_clear(rw_entry);
                        return NO_CONFLICT;
                    } 
                    else {
                        return WRITE_AFTER_READ;
                    }
#else
		    return WRITE_AFTER_READ;
#endif	/* NOCM */
                /* } */
            } 
            else {
                return NO_CONFLICT;
            }
        } 
        /*READING/subscribing ---------------------------------------*/
        else { /*subscribing*/
            if (rw_entry_has_writer(rw_entry)
                    && !rw_entry_is_writer(rw_entry, nodeId)) { /*READ AFTER WRITE*/
                //TODO: here the logic for READ -> WRITE

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
                if (contention_manager(nodeId, &rw_entry->shorts[3], READ_AFTER_WRITE) == TRUE) {
                    //attacker won - old aborted
                    rw_entry_unset_writer(rw_entry);
                    return NO_CONFLICT;
                } 
                else {
                    return READ_AFTER_WRITE;
                }
#else
		return READ_AFTER_WRITE;
#endif	/* NOCM */

            } 
            else {
                return NO_CONFLICT;
            }
        }
    }

INLINED void 
rw_entry_print_readers(rw_entry_t *rwe) 
{
    union {
        unsigned long long int lli;
        unsigned int i[2];
    } convert;

    convert.i[0] = rwe->ints[0];
    convert.i[1] = rwe->shorts[2];
    int i;
    for (i = 0; i < 48; i++) {
        if (convert.lli & 0x01) {
            PRINTS("%d -> ", i);
        }
        convert.lli >>= 1;
    }

    PRINTS("NULL\n");
    FLUSH;
}

#ifdef	__cplusplus
}
#endif

#endif	/* RW_ENTRY_H */
