/* 
 * File:   pubSubTM.c
 * Author: trigonak
 *
 * Created on March 7, 2011, 4:45 PM
 * working
 */

#include "../include/common.h"
#include "../include/pubSubTM.h"
#include <stdio.h> //TODO: clean the includes
#include <unistd.h>
#include <math.h>

#define SENDLIST

iRCCE_WAIT_LIST waitlist;

unsigned int ID; //=RCCE_ue()
unsigned int NUM_UES;
unsigned int NUM_DSL_NODES;
unsigned int *dsl_nodes;
char *buf;
unsigned short nodes_contacted[48];
CONFLICT_TYPE ps_response; //TODO: make it more sophisticated
SHMEM_START_ADDRESS shmem_start_address = NULL;
PS_COMMAND *psc;

int subscribing_address;

static void ps_sendb(unsigned short int target, PS_COMMAND_TYPE operation, unsigned int address, CONFLICT_TYPE response);
static void ps_recvb(unsigned short int from);

inline BOOLEAN shmem_init_start_address();
inline void unsubscribe(int nodeId, int shmem_address);
inline unsigned int shmem_address_offset(void *shmem_address);
inline unsigned int DHT_get_responsible_node(void *shmem_address, unsigned int *address_offset);
inline void publish_finish(int nodeId, int shmem_address);

void ps_init_(void) {
    ID = RCCE_ue();
    NUM_UES = RCCE_num_ues();
    NUM_DSL_NODES = (int) ((NUM_UES / DSLNDPERNODES) + 1);
    PRINTD("NUM_DSL_NODES = %d", NUM_DSL_NODES);
    if ((dsl_nodes = (unsigned  int *) malloc(NUM_DSL_NODES * sizeof (unsigned int))) == NULL) {
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
        if (j % DSLNDPERNODES) {
            dsl_nodes[dsln++] = j;
        }
    }


    iRCCE_init_wait_list(&waitlist);

    RCCE_barrier(&RCCE_COMM_WORLD);
    PRINT("[APP NODE] Initialized pub-sub..");
}

static void ps_sendb(unsigned short int target, PS_COMMAND_TYPE command, unsigned int address, CONFLICT_TYPE response) {

    psc->type = command;
    psc->address = address;
    psc->response = response;

    char data[PS_BUFFER_SIZE];
    
    memcpy(data, psc, sizeof (PS_COMMAND));
    iRCCE_isend(data, PS_BUFFER_SIZE, target, NULL);
}

static void ps_recvb(unsigned short int from) {
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
    ps_sendb(responsible_node, PS_UNSUBSCRIBE, address_offs, NO_CONFLICT);
}

void ps_publish_finish(void *address) {

    unsigned int address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);
    ps_sendb(responsible_node, PS_PUBLISH_FINISH, address_offs, NO_CONFLICT);
}

void ps_finish_all() {

    int i;
    for (i = 0; i < NUM_UES; i++) {
        if (nodes_contacted[i] != 0) { //can be changed to non-blocking
            if (i != ID) {
                ps_sendb(i, PS_REMOVE_NODE, 0, NO_CONFLICT);
            }
            nodes_contacted[i] = 0;
        }
    }
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

inline unsigned int shmem_address_offset(void *shmem_address) {
    return ((int) shmem_address) - shmem_start_address;
}

inline unsigned int DHT_get_responsible_node(void *shmem_address, unsigned int *address_offset) {
    /* shift right by DHT_ADDRESS_MASK, thus making 2^DHT_ADDRESS_MASK continuous
        address handled by the same node*/
    *address_offset = shmem_address_offset(shmem_address);
    return dsl_nodes[(*address_offset >> DHT_ADDRESS_MASK) % NUM_DSL_NODES];
}
