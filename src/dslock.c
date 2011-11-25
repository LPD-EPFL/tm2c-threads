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
extern unsigned int ID; //=RCCE_ue()
extern unsigned int NUM_UES;
unsigned int NUM_UES_APP;
char *buf;
iRCCE_RECV_REQUEST *recv_requests;
iRCCE_RECV_REQUEST *recv_current;
iRCCE_SEND_REQUEST *send_current;
CONFLICT_TYPE ps_response; //TODO: make it more sophisticated
PS_COMMAND *ps_command, *ps_remote, *psc;

#ifdef PGAS
#include "pgas.h"
write_set_pgas_t **PGAS_write_sets;
#endif

unsigned int stats_total = 0, stats_commits = 0, stats_aborts = 0, stats_max_retries = 0, stats_aborts_war = 0,
        stats_aborts_raw = 0, stats_aborts_waw = 0, stats_received = 0;
double stats_duration = 0;

static void dsl_communication();
static inline void ps_send(unsigned short int target, PS_COMMAND_TYPE operation, unsigned int address, CONFLICT_TYPE response);

static inline CONFLICT_TYPE try_subscribe(int nodeId, int shmem_address);
static inline CONFLICT_TYPE try_publish(int nodeId, int shmem_address);
static inline void unsubscribe(int nodeId, int shmem_address);
static inline void publish_finish(int nodeId, int shmem_address);
static void print_global_stats();

void dsl_init(void) {
    NUM_UES_APP = NUM_UES - NUM_DSL_UES;

    ps_command = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    ps_remote = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    psc = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null

    buf = (char *) malloc(NUM_UES * PS_BUFFER_SIZE); //TODO: free at finalize + check for null
    if (ps_command == NULL || ps_remote == NULL || psc == NULL || buf == NULL) {
        PRINTD("malloc ps_command == NULL || ps_remote == NULL || psc == NULL || buf == NULL");
    }

    int i;

#ifdef PGAS
    PGAS_write_sets = (write_set_pgas_t **) malloc(NUM_UES * sizeof (write_set_pgas_t *));
    if (PGAS_write_sets == NULL) {
        PRINT("malloc PGAS_write_sets == NULL");
        EXIT(-1);
    }

    for (i = 0; i < NUM_UES; i++) {
        if (i % DSLNDPERNODES) { /*only for non DSL cores*/
            PGAS_write_sets[i] = write_set_pgas_new();
            if (PGAS_write_sets[i] == NULL) {
                PRINT("malloc PGAS_write_sets[i] == NULL");
                EXIT(-1);
            }
        }
    }

    PGAS_init();

    PRINT("testing ws");
    int k;
    for (k = 0; k < 16; k++) {
        write_set_pgas_insert(PGAS_write_sets[1], k, k);
    }
    
    write_set_pgas_print(PGAS_write_sets[1]);
    PGAS_write_sets[1] = write_set_pgas_empty(PGAS_write_sets[1]);


#endif

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
    for (i = 0; i < NUM_UES; i++) {
        if (i % DSLNDPERNODES) { /*only for non DSL cores*/
            iRCCE_irecv(buf + i * PS_BUFFER_SIZE, PS_BUFFER_SIZE, i, &recv_requests[i]);
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
            base = buf + sender * PS_BUFFER_SIZE;
            ps_remote = (PS_COMMAND *) base;

            switch (ps_remote->type) {
                case PS_SUBSCRIBE:
#ifdef PGAS
                    //PRINT("RL addr: %d", ps_remote->address);
                    ps_send(sender, PS_SUBSCRIBE_RESPONSE, PGAS_read(ps_remote->address), try_subscribe(sender, ps_remote->address));
#else
                    ps_send(sender, PS_SUBSCRIBE_RESPONSE, ps_remote->address, try_subscribe(sender, ps_remote->address));
#endif
                    //ps_send(sender, PS_SUBSCRIBE_RESPONSE, ps_remote->address, NO_CONFLICT);
                    break;
                case PS_PUBLISH:
                {
                    CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);
#ifdef PGAS
                    if (conflict == NO_CONFLICT) {
                        write_set_pgas_insert(PGAS_write_sets[sender], ps_remote->write_value, ps_remote->address);
                    }
#endif
                    ps_send(sender, PS_PUBLISH_RESPONSE, ps_remote->address, conflict);
                    break;
                }
                case PS_REMOVE_NODE:
                    //write_set_pgas_persist(PGAS_write_sets[sender]);
                    write_set_pgas_print(PGAS_write_sets[sender]);
                    PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
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
                        if (RCCE_ue() == 0) {
                            print_global_stats();
                        }
                        EXIT(0);
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
    if (iRCCE_isend(data, PS_BUFFER_SIZE, target, s) != iRCCE_SUCCESS) {
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

    printf("||||||||||||||||||||||||||||||||||||||||||||||||||||||\n");
    printf("TXs Statistics : %02d Nodes, %02d App Nodes|||||||||||||||||||||||\n", NUM_UES, NUM_UES_APP);
    printf(":: TOTAL -----------------------------------------------------\n");
    printf("T | Avg Duration\t: %.3f s\n", stats_duration);
    printf("T | Starts      \t: %lu\n", stats_total);
    printf("T | Commits     \t: %lu\n", stats_commits);
    printf("T | Aborts      \t: %lu\n", stats_aborts);
    printf("T | Max Retries \t: %lu\n", stats_max_retries);
    printf("T | Aborts WAR  \t: %lu\n", stats_aborts_war);
    printf("T | Aborts RAW  \t: %lu\n", stats_aborts_raw);
    printf("T | Aborts WAW  \t: %lu\n", stats_aborts_waw);

    stats_aborts /= stats_duration;
    stats_aborts_raw /= stats_duration;
    stats_aborts_war /= stats_duration;
    stats_aborts_waw /= stats_duration;
    stats_commits /= stats_duration;
    stats_total /= stats_duration;
    int stats_commits_total = stats_commits;

    printf(":: PER SECOND TOTAL AVG --------------------------------------\n");
    printf("TA| Starts      \t: %lu\t/s\n", stats_total);
    printf("TA| Commits     \t: %lu\t/s\n", stats_commits);
    printf("TA| Aborts      \t: %lu\t/s\n", stats_aborts);
    printf("TA| Aborts WAR  \t: %lu\t/s\n", stats_aborts_war);
    printf("TA| Aborts RAW  \t: %lu\t/s\n", stats_aborts_raw);
    printf("TA| Aborts WAW  \t: %lu\t/s\n", stats_aborts_waw);

    int stats_commits_app = stats_commits / NUM_UES_APP;

    stats_aborts /= NUM_UES;
    stats_aborts_raw /= NUM_UES;
    stats_aborts_war /= NUM_UES;
    stats_aborts_waw /= NUM_UES;
    stats_commits /= NUM_UES;
    stats_total /= NUM_UES;

    double commit_rate = (stats_total - stats_aborts) / (double) stats_total;
    double tx_latency = (1 / (double) stats_commits_app) * 1000; //micros

    printf(":: PER SECOND PER NODE AVG -----------------------------------\n");
    printf("NA| Starts      \t: %lu\t/s\n", stats_total);
    printf("NA| Commits     \t: %lu\t/s\n", stats_commits);
    printf("NA| Aborts      \t: %lu\t/s\n", stats_aborts);
    printf("NA| Aborts WAR  \t: %lu\t/s\n", stats_aborts_war);
    printf("NA| Aborts RAW  \t: %lu\t/s\n", stats_aborts_raw);
    printf("NA| Aborts WAW  \t: %lu\t/s\n", stats_aborts_waw);
    printf(":: Collect data ----------------------------------------------\n");
    printf("))) %lu\t%.2f\t %.3f\t(Throughput, Commit Rate, Latency)\n", stats_commits_total, commit_rate * 100, tx_latency);
    printf("||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n");

    fflush(stdout);
}