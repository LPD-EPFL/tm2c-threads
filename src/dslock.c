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

#ifdef SENDLIST
iRCCE_WAIT_LIST sendlist;
#else
BOOLEAN tm_has_command;
#endif
ps_hashtable_t ps_hashtable;
unsigned int ID; //=RCCE_ue()
unsigned int NUM_UES, NUM_UES_APP;
char *buf;
iRCCE_RECV_REQUEST *recv_requests;
iRCCE_RECV_REQUEST *recv_current;
iRCCE_SEND_REQUEST *send_current;
CONFLICT_TYPE ps_response; //TODO: make it more sophisticated
PS_COMMAND *ps_command, *ps_remote, *psc;

unsigned int stats_total = 0, stats_commits = 0, stats_aborts = 0, stats_max_retries = 0, stats_aborts_war = 0,
        stats_aborts_raw = 0, stats_aborts_waw = 0, stats_received = 0;
double stats_duration = 0;

static void dsl_communication();
static inline void ps_send(unsigned short int target, PS_COMMAND_TYPE operation, unsigned int address, CONFLICT_TYPE response);

static inline CONFLICT_TYPE try_subscribe(int nodeId, int shmem_address);
static inline CONFLICT_TYPE try_publish(int nodeId, int shmem_address);
static inline void unsubscribe(int nodeId, int shmem_address);
static inline void publish_finish(int nodeId, int shmem_address);

void dsl_init(void) {
    ID = RCCE_ue();
    NUM_UES = RCCE_num_ues();
    NUM_UES_APP = NUM_UES - NUM_DSL_UES;

    ps_command = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    ps_remote = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    psc = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null

    buf = (char *) malloc(NUM_UES * PS_BUFFER_SIZE); //TODO: free at finalize + check for null
    if (ps_command == NULL || ps_remote == NULL || psc == NULL || buf == NULL) {
        PRINTD("malloc ps_command == NULL || ps_remote == NULL || psc == NULL || buf == NULL");
    }

    ps_hashtable = ps_hashtable_new();

    iRCCE_init_wait_list(&waitlist);
    iRCCE_init_wait_list(&sendlist);

    recv_requests = (iRCCE_RECV_REQUEST*) calloc(NUM_UES, sizeof (iRCCE_RECV_REQUEST));
    if (recv_requests == NULL) {
        fprintf(stderr, "alloc");
        PRINTD("not able to alloc the recv_requests..");
        EXIT(-1);
    }

    // Create recv request for each possible (other) core.
    int i;
    for (i = 0; i < NUM_UES; i++) {
        if (i % DSLNDPERNODES) { /*only for non DSL cores*/
            iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
            iRCCE_add_recv_to_wait_list(&waitlist, &recv_requests[i]);
        }
    }

    RCCE_barrier(&RCCE_COMM_WORLD);
    PRINT("[DSL NODE] Initialized pub-sub..");
    dsl_communication();
}

static void dsl_communication() {
    int sender;
    char *base;

    while (1) {

        iRCCE_test_any(&sendlist, &send_current, NULL);
        if (send_current != NULL) {
            free(send_current->privbuf);
            free(send_current);
            continue;
        }
        //test if any send or recv completed
        iRCCE_test_any(&waitlist, NULL, &recv_current);
        if (recv_current != NULL) {


            //the sender of the message
            sender = recv_current->source;
            base = buf + sender * 32;
            ps_remote = (PS_COMMAND *) base;

            switch (ps_remote->type) {
                case PS_SUBSCRIBE:
                    ps_send(sender, PS_SUBSCRIBE_RESPONSE, ps_remote->address, try_subscribe(sender, ps_remote->address));
                    break;
                case PS_PUBLISH:
                    ps_send(sender, PS_PUBLISH_RESPONSE, ps_remote->address, try_publish(sender, ps_remote->address));
                    break;
                case PS_REMOVE_NODE:
                    ps_hashtable_delete_node(ps_hashtable, sender);
                    break;
                case PS_UNSUBSCRIBE:
                    ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
                    break;
                case PS_PUBLISH_FINISH:
                    ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
                    break;
                case PS_STATS:
                    stats_aborts += ps_remote->aborts;
                    stats_aborts_raw += ps_remote->aborts_raw;
                    stats_aborts_war += ps_remote->aborts_war;
                    stats_aborts_waw += ps_remote->aborts_waw;
                    stats_commits += ps_remote->commits;
                    stats_duration += ps_remote->tx_duration;
                    stats_max_retries = stats_max_retries < ps_remote->max_retries ? ps_remote->max_retries : stats_max_retries;
                    stats_total += ps_remote->commits + ps_remote->aborts;

                    if (++stats_received >= NUM_UES_APP) {
                        print_global_stats();
                    }
                default:
                    PRINTD("REMOTE MSG: ??");
            }

            // Create request for new message from this core, add to waitlist
            iRCCE_irecv(base, PS_BUFFER_SIZE, sender, &recv_requests[sender]);
            iRCCE_add_recv_to_wait_list(&waitlist, &recv_requests[sender]);

        }
    }
}

static inline void ps_send(unsigned short int target, PS_COMMAND_TYPE command, unsigned int address, CONFLICT_TYPE response) {

    iRCCE_SEND_REQUEST *s = (iRCCE_SEND_REQUEST *) malloc(sizeof (iRCCE_SEND_REQUEST));
    if (s == NULL) {
        PRINTD("Could not allocate space for iRCCE_SEND_REQUEST");
        EXIT(-1);
    }
    psc->type = command;
    psc->address = address;
    psc->response = response;

    char *data = (char *) malloc(PS_BUFFER_SIZE * sizeof (char));
    if (data == NULL) {
        PRINTD("Could not allocate space for data");
        EXIT(-1);
    }

    memcpy(data, psc, sizeof (PS_COMMAND));
    if (iRCCE_isend(data, 32, target, s) != iRCCE_SUCCESS) {
        iRCCE_add_send_to_wait_list(&sendlist, s);
        //iRCCE_add_to_wait_list(&sendlist, s, NULL);
    }
    else {
        free(s);
        free(data);
    }
}

static inline CONFLICT_TYPE try_subscribe(int nodeId, int shmem_address) {

    ps_hashtable_insert(ps_hashtable, nodeId, shmem_address, READ);
    return ps_conflict_type;
}

static inline CONFLICT_TYPE try_publish(int nodeId, int shmem_address) {

    ps_hashtable_insert(ps_hashtable, nodeId, shmem_address, WRITE);
    return ps_conflict_type;
}

static inline void unsubscribe(int nodeId, int shmem_address) {
    ps_hashtable_delete(ps_hashtable, nodeId, shmem_address, READ);
}

static inline void publish_finish(int nodeId, int shmem_address) {
    ps_hashtable_delete(ps_hashtable, nodeId, shmem_address, WRITE);
}

static void print_global_stats() {
    stats_duration /= NUM_UES_APP;

    printf("||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n");
    printf("TXs Statistics : %02d Nodes, %02d App Nodes|||||||||||||||||||||||\n", NUM_UES, NUM_UES_APP);
    printf(":: TOTAL -----------------------------------------------------\n");
    printf("Avg Duration\t: %.3f s\n", stats_duration);
    printf("Starts      \t: %lu\n", stats_total);
    printf("Commits     \t: %lu\n", stats_commits);
    printf("Aborts      \t: %lu\n", stats_aborts);
    printf("Max Retries \t: %lu\n", stats_max_retries);
    printf("Aborts WAR  \t: %lu\n", stats_aborts_war);
    printf("Aborts RAW  \t: %lu\n", stats_aborts_raw);
    printf("Aborts WAW  \t: %lu\n", stats_aborts_waw);
    printf("--------------------------------------------------------------\n");

    stats_aborts /= stats_duration;
    stats_aborts_raw /= stats_duration;
    stats_aborts_war /= stats_duration;
    stats_aborts_waw /= stats_duration;
    stats_commits /= stats_duration;
    stats_total /= stats_duration;

    printf(":: PER SECOND TOTAL AVG --------------------------------------\n");
    printf("Starts      \t: %lu\t/s\n", stats_total);
    printf("Commits     \t: %lu\t/s\n", stats_commits);
    printf("Aborts      \t: %lu\t/s\n", stats_aborts);
    printf("Aborts WAR  \t: %lu\t/s\n", stats_aborts_war);
    printf("Aborts RAW  \t: %lu\t/s\n", stats_aborts_raw);
    printf("Aborts WAW  \t: %lu\t/s\n", stats_aborts_waw);
    printf("--------------------------------------------------------------\n");

    stats_aborts /= NUM_UES_APP;
    stats_aborts_raw /= NUM_UES_APP;
    stats_aborts_war /= NUM_UES_APP;
    stats_aborts_waw /= NUM_UES_APP;
    stats_commits /= NUM_UES_APP;
    stats_total /= NUM_UES_APP;


    double commit_rate = (stats_total - stats_aborts) / (double) stats_total;
    double tx_latency = (1 / (double) stats_commits) * 1000; //micros

    printf(":: PER SECOND PER NODE AVG -----------------------------------\n");
    printf("Starts      \t: %lu\t/s\n", stats_total);
    printf("Commits     \t: %lu\t/s\n", stats_commits);
    printf("Aborts      \t: %lu\t/s\n", stats_aborts);
    printf("Aborts WAR  \t: %lu\t/s\n", stats_aborts_war);
    printf("Aborts RAW  \t: %lu\t/s\n", stats_aborts_raw);
    printf("Aborts WAW  \t: %lu\t/s\n", stats_aborts_waw);
    printf("--\n");
    printf("Commit Rate \t: %.2f %%\n", commit_rate * 100);
    printf("Latency     \t: %.3f ms\n", tx_latency);
    printf("--------------------------------------------------------------\n");
    printf("||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n");

    fflush(stdout);
}