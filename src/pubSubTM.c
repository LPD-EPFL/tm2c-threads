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
//#define iRCCE32

char data[32];
iRCCE_WAIT_LIST waitlist;

#ifdef SENDLIST
iRCCE_WAIT_LIST sendlist;
#else
BOOLEAN tm_has_command;
#endif
ps_hashtable_t ps_hashtable;
unsigned int ID; //=RCCE_ue()
unsigned int NUM_UES;
char *buf;
unsigned short nodes_contacted[48];
iRCCE_RECV_REQUEST *recv_requests;
iRCCE_RECV_REQUEST *recv_current;
iRCCE_SEND_REQUEST *send_current;
BOOLEAN ps_has_response;
CONFLICT_TYPE ps_response; //TODO: make it more sophisticated
PS_COMMAND *ps_command, *ps_remote, *psc;
SHMEM_START_ADDRESS shmem_start_address = NULL;

int subscribing_address;

#ifdef TASKLIB
Rendez *ps_has_response_rendez;
#else
pthread_mutex_t ps_has_response_mutex;
pthread_cond_t ps_has_response_cond;
#endif

static void ps_communication(void *);
static void ps_send(unsigned short int target, PS_COMMAND_TYPE operation, unsigned int address, CONFLICT_TYPE response);

inline BOOLEAN shmem_init_start_address();
inline void set_command(PS_COMMAND_TYPE command_type, unsigned int address, unsigned short int target /* = 0 */);
inline void wait_ps(void);
inline void ps_signal_has_response();
inline CONFLICT_TYPE try_subscribe(int nodeId, int shmem_address);
inline CONFLICT_TYPE try_publish(int nodeId, int shmem_address);
inline void unsubscribe(int nodeId, int shmem_address);
inline unsigned int shmem_address_offset(void *shmem_address);
inline unsigned int DHT_get_responsible_node(void *shmem_address, unsigned int *address_offset);
inline void publish_finish(int nodeId, int shmem_address);

void ps_init_(void) {

    iRCCE_init();

    ID = RCCE_ue();
    NUM_UES = RCCE_num_ues();
#ifndef SENDLIST
    tm_has_command = FALSE;
#endif
    ps_command = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    ps_remote = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    psc = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    buf = (char *) malloc(NUM_UES * PS_BUFFER_SIZE); //TODO: free at finalize + check for null
    if (ps_command == NULL || ps_remote == NULL || psc == NULL || buf == NULL) {
        PRINTD("malloc ps_command == NULL || ps_remote == NULL || psc == NULL || buf == NULL");
    }
    shmem_init_start_address();
    ps_hashtable = ps_hashtable_new();
    int j;
    for (j = 0; j < NUM_UES; j++) {
        nodes_contacted[j] = 0;
    }

#ifdef TASKLIB
    //TODO: free at finalize
    if ((ps_has_response_rendez = (Rendez *) malloc(sizeof (Rendez))) == NULL) {
        PRINTD("malloc ps_has_response_rendez");
    }
#else
    pthread_mutex_init(&ps_has_response_mutex, NULL); //TODO: destroy at finalize
    pthread_cond_init(&ps_has_response_cond, NULL); //TODO: destroy at finalize
#endif

    iRCCE_init_wait_list(&waitlist);
#ifdef SENDLIST
    iRCCE_init_wait_list(&sendlist);
#endif

    recv_requests = (iRCCE_RECV_REQUEST*) calloc(NUM_UES, sizeof (iRCCE_RECV_REQUEST));
    if (recv_requests == NULL) {
        fprintf(stderr, "alloc");
        PRINTD("not able to alloc the recv_requests..");
#ifdef TASKLIB
        taskexit(-1);
#else
        pthread_exit((void *) - 1);
#endif
    }

    // Create recv request for each possible (other) core.
    int i;
    for (i = 0; i < NUM_UES; i++) {
        if (i != ID) {
#ifndef iRCCE32
            iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
#else
            iRCCE_irecv32(buf + i * 32, i, &recv_requests[i]);
#endif
            iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[i]);
        }
    }

    RCCE_barrier(&RCCE_COMM_WORLD);
    taskcreate(ps_communication, (void *) NULL, PS_TASK_STACK_SIZE);
    PRINT("Initialized pub-sub..");
}

static void ps_communication(void *args) {
    taskname("pssrcv");

    while (1) {
#ifndef SENDLIST
        //TODO: move in a separate function
        if (tm_has_command) {
            //send the request
            ps_send(ps_command->target, (PS_COMMAND_TYPE) ps_command->type, ps_command->address, FALSE);

#ifndef TASKLIB
            pthread_mutex_lock(&ps_has_response_mutex);
#endif

            tm_has_command = FALSE;
        }
#endif

        //TODO: try not to execute it in every run for increasing the performance
#ifdef SENDLIST
        iRCCE_test_any(&sendlist, &send_current, NULL);
        if (send_current != NULL) {
            free(send_current->privbuf);
            free(send_current);

            continue;
        }
#endif

        //test if any send or recv completed
        iRCCE_test_any(&waitlist, NULL, &recv_current);
#ifndef SENDLIST
        if (send_current != NULL) {
            free(send_current->privbuf);
            free(send_current);

        }
        else
#endif
            if (recv_current != NULL) {

            //the sender of the message
            int sender = recv_current->source;
            char *base = buf + sender * 32;
            ps_remote = (PS_COMMAND *) base;

            switch (ps_remote->type) {
                case PS_SUBSCRIBE:
                    ps_send(sender, PS_SUBSCRIBE_RESPONSE, ps_remote->address, try_subscribe(sender, ps_remote->address));
                    break;
                case PS_SUBSCRIBE_RESPONSE:
                    if (subscribing_address != ps_remote->address) {
                        PRINT("SUB(%d) \t-> RESP(%d)\t. Node(%2d)", subscribing_address, ps_remote->address, sender);
                    }
                    ps_response = (CONFLICT_TYPE) ps_remote->response;
                    ps_signal_has_response();
                    break;
                case PS_PUBLISH:
                    ps_send(sender, PS_PUBLISH_RESPONSE, ps_remote->address, try_publish(sender, ps_remote->address));
                    break;
                case PS_PUBLISH_RESPONSE:
                    ps_response = (CONFLICT_TYPE) ps_remote->response;
                    ps_signal_has_response();
                    break;
                case PS_UNSUBSCRIBE:
                    unsubscribe(sender, ps_remote->address); //TODO: do it in a new task??
                    break;
                case PS_PUBLISH_FINISH:
                    publish_finish(sender, ps_remote->address); //TODO: finish publishing
                    break;
                case PS_ABORTED:
                    //TODO: aborted by a contention manager
                    break;
                case PS_REMOVE_NODE:
                    //PRINTD("\t\t\t\tPS_REMOVE_NODE from [%d]", sender);
                    ps_hashtable_delete_node(ps_hashtable, sender);
                    //TODO: send response??
                    break;
                default:
                    PRINTD("REMOTE MSG: ??");
            }


            ////PRINTD("REMOTE MSG: end");

            // Create request for new message from this core, add to waitlist
#ifndef iRCCE32
            iRCCE_irecv(base, 32, sender, &recv_requests[sender]);
#else
            iRCCE_irecv32(base, sender, &recv_requests[sender]);
#endif
            iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[sender]);

        }
#ifdef TASKLIB
        else {
            taskndelay(1);
        }
#endif

    }
}

static void ps_send(unsigned short int target, PS_COMMAND_TYPE command, unsigned int address, CONFLICT_TYPE response) {

    iRCCE_SEND_REQUEST *s = (iRCCE_SEND_REQUEST *) malloc(sizeof (iRCCE_SEND_REQUEST));
    if (s == NULL) {
        PRINTD("Could not allocate space for iRCCE_SEND_REQUEST");
#ifdef TASKLIB
        taskexit(-1);
#else
        pthread_exit((void *) - 1);
#endif
    }
    psc->type = command;
    psc->address = address;
    psc->response = response;

    char *data = (char *) malloc(PS_BUFFER_SIZE * sizeof (char));
    if (data == NULL) {
        PRINTD("Could not allocate space for data");
#ifdef TASKLIB
        taskexit(-1);
#else
        pthread_exit((void *) - 1);
#endif
    }
    
    memcpy(data, psc, sizeof (PS_COMMAND));
/*
    RC_cache_invalidate();
*/
    if (iRCCE_isend(data, 32, target, s) != iRCCE_SUCCESS) {

#ifdef SENDLIST
        iRCCE_add_to_wait_list(&sendlist, s, NULL);
#else
        iRCCE_add_to_wait_list(&waitlist, s, NULL);
#endif
    }
    else {
        //PRINTD("Freed on the fly!");
        free(s);
        free(data);
    }
}

inline void iRCCE_ipush() {
    iRCCE_isend_push();
    iRCCE_irecv_push();
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

    if (responsible_node == ID) {
        return try_subscribe(ID, address_offs);
    }

#ifndef SENDLIST
    set_command(PS_SUBSCRIBE, address_offs, responsible_node);
#else
    ps_send(responsible_node, PS_SUBSCRIBE, address_offs, NO_CONFLICT);
#endif

    wait_ps();

    return ps_response;
}

CONFLICT_TYPE ps_publish(void *address) {

    unsigned int address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);

    nodes_contacted[responsible_node]++;

    if (responsible_node == ID) {
        return try_publish(ID, address_offs);
    }

#ifndef SENDLIST
    set_command(PS_PUBLISH, address_offs, responsible_node);
#else
    ps_send(responsible_node, PS_PUBLISH, address_offs, NO_CONFLICT);
#endif
    wait_ps();

    return ps_response;
}

void ps_unsubscribe(void *address) {

    unsigned int address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);
    if (responsible_node == ID) {
        ps_hashtable_delete(ps_hashtable, ID, address_offs, READ);
        return;
    }

#ifndef SENDLIST
    set_command(PS_UNSUBSCRIBE, address_offs, responsible_node);
    tm_has_command = TRUE;
#else
    ps_send(responsible_node, PS_UNSUBSCRIBE, address_offs, NO_CONFLICT);
#endif

}

void ps_publish_finish(void *address) {

    unsigned int address_offs;
    unsigned short int responsible_node = DHT_get_responsible_node(address, &address_offs);
    if (responsible_node == ID) {
        ps_hashtable_delete(ps_hashtable, ID, address_offs, WRITE);
        return;
    }

#ifndef SENDLIST
    set_command(PS_PUBLISH_FINISH, address_offs, responsible_node);
    tm_has_command = TRUE;
#else
    ps_send(responsible_node, PS_PUBLISH_FINISH, address_offs, NO_CONFLICT);
#endif

}

void ps_finish_all() {

    int i;
    for (i = 0; i < NUM_UES; i++) {
        if (nodes_contacted[i] != 0) {
            if (i != ID) {
#ifndef SENDLIST
                set_command(PS_REMOVE_NODE, 0, 0);
                tm_has_command = TRUE;
#else
                //PRINTD("PS_REMOVE_NODE to %d (nc: %d)", i, nodes_contacted[i]);
                ps_send(i, PS_REMOVE_NODE, 0, NO_CONFLICT);
#endif
            }
            else {
                //PRINTD("PS_REMOVE_NODE SELF (ID: %d, nc: %d)", ID, nodes_contacted[i]);
                ps_hashtable_delete_node(ps_hashtable, ID);
            }
            nodes_contacted[i] = 0;
        }
        //taskyield();
    }
}

/*
 * ____________________________________________________________________________________________
 Help functions _______________________________________________________________________________|
 * ____________________________________________________________________________________________|
 */

inline void set_command(PS_COMMAND_TYPE command_type, unsigned int address, unsigned short int target /* = 0 */) {
    ps_command->type = command_type;
    ps_command->address = address;
    ps_command->target = target;

    //tm_has_command = TRUE;
}

inline void wait_ps() {
#ifdef TASKLIB
#ifndef SENDLIST
    tm_has_command = TRUE;
#endif
    tasksleep(ps_has_response_rendez);
#else
    pthread_mutex_lock(&ps_has_response_mutex);
    tm_has_command = TRUE;
    pthread_cond_wait(&ps_has_response_cond, &ps_has_response_mutex);
    pthread_mutex_unlock(&ps_has_response_mutex);
#endif
}

inline void ps_signal_has_response() {
#ifdef TASKLIB
    taskwakeup(ps_has_response_rendez);
#else
    pthread_cond_signal(&ps_has_response_cond);
    pthread_mutex_unlock(&ps_has_response_mutex);
#endif
}

inline CONFLICT_TYPE try_subscribe(int nodeId, int shmem_address) {

    ps_hashtable_insert(ps_hashtable, nodeId, shmem_address, READ);
    return ps_conflict_type;
}

inline CONFLICT_TYPE try_publish(int nodeId, int shmem_address) {

    ps_hashtable_insert(ps_hashtable, nodeId, shmem_address, WRITE);
    return ps_conflict_type;
}

inline void unsubscribe(int nodeId, int shmem_address) {
    ps_hashtable_delete(ps_hashtable, nodeId, shmem_address, READ);
}

inline void publish_finish(int nodeId, int shmem_address) {
    ps_hashtable_delete(ps_hashtable, nodeId, shmem_address, WRITE);
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
    return ((int) shmem_address) -shmem_start_address;
}

inline unsigned int DHT_get_responsible_node(void *shmem_address, unsigned int *address_offset /* = 0 */) {
    /* shift right by DHT_ADDRESS_MASK, thus making 2^DHT_ADDRESS_MASK continuous
        address handled by the same node*/
    *address_offset = shmem_address_offset(shmem_address);
    return (*address_offset >> DHT_ADDRESS_MASK) % NUM_UES;
}
