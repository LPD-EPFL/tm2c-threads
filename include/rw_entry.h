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


/*__________________________________________________________________________________________________
* RW ENTRY                                                                                         |
* _________________________________________________________________________________________________|
*/

#define NO_WRITERI 0x10000000
#define NO_WRITER 0x1000
#define NUM_OF_BUCKETS 47 


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
    return (BOOLEAN) ((rwe->ints[(int) (nodeId / 32)] >> (nodeId % 32)) & 0x01);
}

INLINED void
rw_entry_set(rw_entry_t *rwe, nodeid_t nodeId)
{
    rwe->ints[(int) (nodeId / 32)] |= (1 << (nodeId % 32));
}

INLINED void
rw_entry_unset(rw_entry_t *rwe, nodeid_t nodeId)
{
    rwe->ints[(int) (nodeId / 32)] &= (~(1 << (nodeId % 32)));
}

INLINED void
rw_entry_clear(rw_entry_t *rwe)
{
    rwe->ints[0] = 0;
    rwe->shorts[3] = NO_WRITER;
}

INLINED BOOLEAN
rw_entry_is_empty(rw_entry_t *rwe)
{
    return (BOOLEAN) (rwe->ints[0] == 0 && rwe->ints[1] == NO_WRITERI);
}

INLINED BOOLEAN
rw_entry_is_unique_reader(rw_entry_t *rwe, nodeid_t nodeId)
{

    union {
        unsigned long long int lli;
        unsigned int i[2];
        unsigned short s[4];
    } convert;
    convert.lli = 0;
    convert.i[(int) (nodeId / 32)] = (1 << (nodeId % 32));
    return (BOOLEAN) (convert.i[0] == rwe->ints[0] && convert.s[2] == rwe->shorts[2]);
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
    rw_entry_t *r = (rw_entry_t *) calloc(1, sizeof (rw_entry_t));
    if (r == NULL) {
        PRINTD("malloc r @ rw_entry_new");
        return NULL;
    }
    r->shorts[3] = NO_WRITER;
    return r;
}

/*
 * Detect whether the new insert into the entry would cause a possible
 * READ/WRITE, WRITE/READ, and WRITE/WRITE conflict.
 */
INLINED CONFLICT_TYPE
rw_entry_is_conflicting(rw_entry_t* rw_entry, nodeid_t nodeId, RW rw)
{
    if (rw_entry == NULL) {
        return NO_CONFLICT;
    }

    if (rw == WRITE) { /*publishing*/
        if (rw_entry_has_writer(rw_entry)) { /*WRITE/WRITE conflict*/
            return WRITE_AFTER_WRITE;
        } else if (!rw_entry_is_empty(rw_entry)) { /*Possible READ/WRITE*/
            /*if the only writer is the one that "asks"*/
            if (rw_entry_is_unique_reader(rw_entry, nodeId)) {
                return NO_CONFLICT;
            } else { /*READ/WRITE conflict*/
                return WRITE_AFTER_READ;
            }
        } else {
            return NO_CONFLICT;
        }
    } else { /*subscribing*/
        if (rw_entry_has_writer(rw_entry) 
        	&& !rw_entry_is_writer(rw_entry, nodeId)) { /*WRITE/READ*/
            return READ_AFTER_WRITE;
        } else {
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
