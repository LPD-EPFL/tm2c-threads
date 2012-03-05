/* 
 * File:   pubSubTM.c
 * Author: trigonak
 *
 * Created on March 7, 2011, 4:45 PM
 * 
 */

#include "../include/common.h"
#include "../include/pubSubTM.h"

unsigned int *dsl_nodes;
unsigned short nodes_contacted[48];
CONFLICT_TYPE ps_response; //TODO: make it more sophisticated

tm_addr_t shmem_start_address = NULL;

PS_COMMAND *psc;

int read_value;

static inline void ps_sendb(unsigned short int target, PS_COMMAND_TYPE operation,
							tm_addr_t address, CONFLICT_TYPE response);
static inline void ps_recvb(unsigned short int from);

inline BOOLEAN shmem_init_start_address();
inline void unsubscribe(nodeid_t nodeId, tm_addr_t shmem_address);

static inline tm_addr_t shmem_address_offset(tm_addr_t shmem_address);
static inline unsigned int DHT_get_responsible_node(tm_addr_t shmem_address, tm_addr_t* address_offset);

inline void publish_finish(nodeid_t nodeId, tm_addr_t shmem_address);

void ps_init_(void) {
    PRINTD("NUM_DSL_NODES = %d", NUM_DSL_NODES);
    if ((dsl_nodes = (unsigned int *) malloc(NUM_DSL_NODES * sizeof (unsigned int))) == NULL) {
        PRINT("malloc dsl_nodes");
        EXIT(-1);
    }

    psc = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    if (psc == NULL) {
        PRINT("malloc psc == NULL");
    }

    shmem_init_start_address();

    int dsln = 0;
    unsigned int j;
    for (j = 0; j < NUM_UES; j++) {
        nodes_contacted[j] = 0;
        if (j % DSLNDPERNODES == 0) {
            dsl_nodes[dsln++] = j;
        }
    }

    sys_ps_init_();
    PRINT("[APP NODE] Initialized pub-sub..");
}

#ifdef PGAS

static inline void ps_send_rl(unsigned short int target, tm_addr_t address) {

    psc->type = PS_SUBSCRIBE;
    psc->address = (uintptr_t)address;

    sys_sendcmd(psc, sizeof(PS_COMMAND), target);
}

static inline void ps_send_wl(unsigned short int target, tm_addr_t address, int value) {

    psc->type = PS_PUBLISH;
    psc->address = (uintptr_t)address;
    psc->write_value = value;

    sys_sendcmd(psc, sizeof(PS_COMMAND), target);
}

static inline void ps_send_winc(unsigned short int target, tm_addr_t address, int value) {

    psc->type = PS_WRITE_INC;
    psc->address = (uintptr_t)address;
    psc->write_value = value;

    sys_sendcmd(psc, sizeof(PS_COMMAND), target);
}

static inline void ps_recv_wl(unsigned short int from) {
    // XXX: this could be written much better, without globals
    PS_COMMAND cmd; 

    sys_recvcmd(&cmd, sizeof(PS_COMMAND), from);
    ps_response = cmd.response;
}
#endif

static inline void 
ps_sendb(unsigned short int target, PS_COMMAND_TYPE command,
		tm_addr_t address, CONFLICT_TYPE response)
{

    psc->type = command;
    psc->address = (uintptr_t)address;
    psc->response = response;

    sys_sendcmd(psc, sizeof(PS_COMMAND), target);
}

static inline void ps_recvb(unsigned short int from) {
    // XXX: this could be written much better, without globals
    PS_COMMAND cmd; 

    sys_recvcmd(&cmd, sizeof(PS_COMMAND), from);
    ps_response = cmd.response;
#ifdef PGAS
    PF_START(0)
    read_value = cmd.value;
    PF_STOP(0)
#endif
}

/*
 * ____________________________________________________________________________________________
 TM interface _________________________________________________________________________________|
 * ____________________________________________________________________________________________|
 */

CONFLICT_TYPE ps_subscribe(tm_addr_t address) {
    tm_addr_t address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]++;

    //PF_START(1)
#ifdef PGAS
    //ps_send_rl(responsible_node, (unsigned int) address);
    ps_send_rl(responsible_node, SHRINK(address));
#else
    //PF_START(2)
    ps_sendb(responsible_node, PS_SUBSCRIBE, address_offs, NO_CONFLICT);
    //PF_STOP(2)

#endif
    //    PRINTD("[SUB] addr: %d to %02d", address_offs, responsible_node);
    //PF_START(3)
    ps_recvb(responsible_node);
    //PF_STOP(3)
    // PF_STOP(1)

    return ps_response;
}

#ifdef PGAS

CONFLICT_TYPE ps_publish(tm_addr_t address, int value) {
#else

CONFLICT_TYPE ps_publish(tm_addr_t address) {
#endif

    tm_addr_t address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]++;

#ifdef PGAS
    ps_send_wl(responsible_node, SHRINK(address), value);
    ps_recv_wl(responsible_node);
#else
    ps_sendb(responsible_node, PS_PUBLISH, address_offs, NO_CONFLICT); //make sync
    ps_recvb(responsible_node);

#endif

    return ps_response;
}

#ifdef PGAS

CONFLICT_TYPE ps_store_inc(tm_addr_t address, int increment) {
    tm_addr_t address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);
    nodes_contacted[responsible_node]++;

    ps_send_winc(responsible_node, SHRINK(address), increment);
    ps_recv_wl(responsible_node);

    return ps_response;
}
#endif

void ps_unsubscribe(tm_addr_t address) {
    tm_addr_t address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]--;
#ifdef PGAS
    ps_sendb(responsible_node, PS_UNSUBSCRIBE, SHRINK(address), NO_CONFLICT);
#else
    ps_sendb(responsible_node, PS_UNSUBSCRIBE, address_offs, NO_CONFLICT);
#endif
}

void ps_publish_finish(tm_addr_t address) {

    tm_addr_t address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]--;

#ifdef PGAS
    ps_sendb(responsible_node, PS_PUBLISH_FINISH, SHRINK(address), NO_CONFLICT);
#else
    ps_sendb(responsible_node, PS_PUBLISH_FINISH, address_offs, NO_CONFLICT);
#endif
}

void ps_finish_all(CONFLICT_TYPE conflict) {
#define FINISH_ALL_PARALLEL_
#ifdef FINISH_ALL_PARALLEL
    iRCCE_SEND_REQUEST sends[NUM_UES];
    char data[PS_BUFFER_SIZE];
    psc->type = PS_REMOVE_NODE;
    psc->response = conflict;
    memcpy(data, psc, sizeof (PS_COMMAND));
#endif

    unsigned int i;
    for (i = 0; i < NUM_UES; i++) {
        if (nodes_contacted[i] != 0) { //can be changed to non-blocking

#ifndef FINISH_ALL_PARALLEL
            ps_sendb(i, PS_REMOVE_NODE, 0, conflict);
#else
            if (iRCCE_isend(data, PS_BUFFER_SIZE, i, &sends[i]) != iRCCE_SUCCESS) {
                iRCCE_add_send_to_wait_list(&waitlist, &sends[i]);
            }
#endif
            nodes_contacted[i] = 0;
        }
    }

#ifdef FINISH_ALL_PARALLEL
    iRCCE_wait_all(&waitlist);
#endif

}

void ps_send_stats(stm_tx_node_t* stats, double duration) {
    psc->type = PS_STATS;

    psc->aborts = stats->tx_aborted;
    psc->aborts_raw = stats->aborts_raw;
    psc->aborts_war = stats->aborts_war;
    psc->aborts_waw = stats->aborts_waw;
    psc->commits = stats->tx_commited;
    psc->max_retries = stats->max_retries;
    psc->tx_duration = duration;

    char data[PS_BUFFER_SIZE];

    memcpy(data, psc, sizeof (PS_COMMAND));
    int i;
    for (i = 0; i < NUM_DSL_UES; i++) {
        iRCCE_isend(data, PS_BUFFER_SIZE, dsl_nodes[i], NULL);
    }
}

/*
 * ____________________________________________________________________________________________
 "DHT"  functions _____________________________________________________________________________|
 * ____________________________________________________________________________________________|
 */

inline BOOLEAN shmem_init_start_address() {
    if (shmem_start_address == NULL) {
        char *start = (char *) sys_shmalloc(sizeof (char));
        if (start == NULL) {
            PRINTD("shmalloc shmem_init_start_address");
        }
        shmem_start_address = (tm_addr_t) start;
        sys_shfree((volatile unsigned char *) start);
        return TRUE;
    }

    return FALSE;
}

static inline tm_addr_t shmem_address_offset(tm_addr_t shmem_address) {
    return (tm_addr_t)((uintptr_t)shmem_address - (uintptr_t)shmem_start_address);
}

static inline unsigned int DHT_get_responsible_node(tm_addr_t shmem_address, tm_addr_t* address_offset) {
    /* shift right by DHT_ADDRESS_MASK, thus making 2^DHT_ADDRESS_MASK continuous
        address handled by the same node*/
#ifdef PGAS
    return dsl_nodes[(uintptr_t)shmem_address % NUM_DSL_NODES];
#else
    *address_offset = shmem_address_offset(shmem_address);
    return dsl_nodes[((uintptr_t)(*address_offset) >> DHT_ADDRESS_MASK) % NUM_DSL_NODES];

#endif

}
