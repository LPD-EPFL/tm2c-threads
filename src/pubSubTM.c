/* 
 * File:   pubSubTM.c
 * Author: trigonak
 *
 * Created on March 7, 2011, 4:45 PM
 * 
 */

#include "../include/common.h"
#include "../include/pubSubTM.h"

#define SENDLIST

iRCCE_WAIT_LIST waitlist;

extern unsigned int ID; //=RCCE_ue()
extern unsigned int NUM_UES;
unsigned int NUM_DSL_NODES;
unsigned int *dsl_nodes;
char *buf;
unsigned short nodes_contacted[48];
CONFLICT_TYPE ps_response; //TODO: make it more sophisticated
SHMEM_START_ADDRESS shmem_start_address = NULL;
PS_COMMAND *psc;

int subscribing_address;

static inline void ps_sendb(unsigned short int target, PS_COMMAND_TYPE operation, unsigned int address, CONFLICT_TYPE response);
static inline void ps_recvb(unsigned short int from);

inline BOOLEAN shmem_init_start_address();
inline void unsubscribe(int nodeId, int shmem_address);
static inline unsigned int shmem_address_offset(void *shmem_address);
static inline unsigned int DHT_get_responsible_node(void *shmem_address, unsigned int *address_offset);
inline void publish_finish(int nodeId, int shmem_address);

void ps_init_(void) {
    NUM_DSL_NODES = (int) ((NUM_UES / DSLNDPERNODES)) + (NUM_UES % DSLNDPERNODES ? 1 : 0);
    PRINTD("NUM_DSL_NODES = %d", NUM_DSL_NODES);
    if ((dsl_nodes = (unsigned int *) malloc(NUM_DSL_NODES * sizeof (unsigned int))) == NULL) {
        PRINTD("malloc dsl_nodes");
        EXIT(-1);
    }

    psc = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    buf = (char *) malloc(NUM_UES * PS_BUFFER_SIZE); //TODO: free at finalize + check for null
    if (psc == NULL || buf == NULL) {
        PRINTD("malloc ps_command == NULL || ps_remote == NULL || psc == NULL || buf == NULL");
    }
    shmem_init_start_address();
    int j, dsln = 0;
    for (j = 0; j < NUM_UES; j++) {
        nodes_contacted[j] = 0;
        if (j % DSLNDPERNODES == 0) {
            dsl_nodes[dsln++] = j;
        }
    }


    iRCCE_init_wait_list(&waitlist);

    RCCE_barrier(&RCCE_COMM_WORLD);
    PRINT("[APP NODE] Initialized pub-sub..");
}

static inline void ps_send(unsigned short int target, PS_COMMAND_TYPE command, unsigned int address,
        iRCCE_SEND_REQUEST *s, char * data) {


}

static inline void ps_sendb(unsigned short int target, PS_COMMAND_TYPE command, unsigned int address, CONFLICT_TYPE response) {

    psc->type = command;
    psc->address = address;
    psc->response = response;

    char data[PS_BUFFER_SIZE];

    memcpy(data, psc, sizeof (PS_COMMAND));
    iRCCE_isend(data, PS_BUFFER_SIZE, target, NULL);
}

static inline void ps_recvb(unsigned short int from) {
    char data[PS_BUFFER_SIZE];
    iRCCE_irecv(data, PS_BUFFER_SIZE, from, NULL);
    PS_COMMAND * cmd = (PS_COMMAND *) data;
    ps_response = cmd->response;
}

/*
 * ____________________________________________________________________________________________
 TM interface _________________________________________________________________________________|
 * ____________________________________________________________________________________________|
 */

CONFLICT_TYPE ps_subscribe(void *address) {

    unsigned int address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]++;

    subscribing_address = address_offs;

    ps_sendb(responsible_node, PS_SUBSCRIBE, address_offs, NO_CONFLICT);
    //    PRINTD("[SUB] addr: %d to %02d", address_offs, responsible_node);
    ps_recvb(responsible_node);

    return ps_response;
}

CONFLICT_TYPE ps_publish(void *address) {

    unsigned int address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]++;

    ps_sendb(responsible_node, PS_PUBLISH, address_offs, NO_CONFLICT); //make sync
    ps_recvb(responsible_node);

    return ps_response;
}

void ps_unsubscribe(void *address) {

    unsigned int address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]--;

    ps_sendb(responsible_node, PS_UNSUBSCRIBE, address_offs, NO_CONFLICT);
}

void ps_publish_finish(void *address) {

    unsigned int address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]--;

    ps_sendb(responsible_node, PS_PUBLISH_FINISH, address_offs, NO_CONFLICT);
}

void ps_finish_all() {
#define FINISH_ALL_PARALLEL
    int i;

#ifdef FINISH_ALL_PARALLEL
    iRCCE_SEND_REQUEST sends[NUM_UES];
    char data[PS_BUFFER_SIZE];
    psc->type = PS_REMOVE_NODE;
    memcpy(data, psc, sizeof (PS_COMMAND));
#endif
    
    for (i = 0; i < NUM_UES; i++) {
        if (nodes_contacted[i] != 0) { //can be changed to non-blocking

#ifndef FINISH_ALL_PARALLEL
            ps_sendb(i, PS_REMOVE_NODE, 0, NO_CONFLICT);
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
    iRCCE_isend(data, PS_BUFFER_SIZE, 0, NULL);
}

/*
 * ____________________________________________________________________________________________
 "DHT"  functions _____________________________________________________________________________|
 * ____________________________________________________________________________________________|
 */

inline BOOLEAN shmem_init_start_address() {
    if (!shmem_start_address) {
        char *start = (char *) RCCE_shmalloc(sizeof (char));
        if (start == NULL) {
            PRINTD("shmalloc shmem_init_start_address");
        }
        shmem_start_address = (SHMEM_START_ADDRESS) start;
        RCCE_shfree((volatile unsigned char *) start);
        return TRUE;
    }

    return FALSE;
}

static inline unsigned int shmem_address_offset(void *shmem_address) {
    return ((int) shmem_address) -shmem_start_address;
}

static inline unsigned int DHT_get_responsible_node(void *shmem_address, unsigned int *address_offset) {
    /* shift right by DHT_ADDRESS_MASK, thus making 2^DHT_ADDRESS_MASK continuous
        address handled by the same node*/
    *address_offset = shmem_address_offset(shmem_address);
    return dsl_nodes[(*address_offset >> DHT_ADDRESS_MASK) % NUM_DSL_NODES];
}
