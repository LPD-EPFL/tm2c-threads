/*
 * File:   iRCCE_all.c
 * Author: Vasileios Trigonakis
 *
 * Created on May 30, 2011, 5:45 PM
 * 
 * Each core (core_a) randomly selects another (core_b) and sends a REQUEST message containing an integer. On receive, 
 * core_b sends back a RESPONSE message to core_a, containing the integer of the REQUEST. 
 * core_a validates the RESPONSE against this integer.
 * 
 * This process is executed repeatedly.
 * 
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "iRCCE.h"


/*printing macros*/
#define ME printf("[%02d] ", RCCE_ue())
#define PRINTD(args...) {ME; printf(args); printf("\n"); fflush(stdout);}

/*function headers*/
void listen(int repeats);
void send(int, int);
inline long rand_range(long r);
inline void srand_core();

/*variables*/
int rcounter = 0; /*receives counter  */
int scounter = 0; /*sends counter*/
int snccounter = 0; /*sends including the ones queued*/
int print_success = 1; /*used to print the success message only once*/
unsigned int number_sent = 0; /*the last integer that was send - used for validating the RESPONSE*/
unsigned int number_sent_to = -1; /*the core to which the last REQUEST was sent*/
unsigned int to_send = 1; /*if a new REQUEST should be sent (after the previous one is completed)*/
iRCCE_WAIT_LIST waitlist, sendlist; /*incoming & outgoing msgs waitlists*/

/*REQUEST or RESPONSE type*/
typedef enum {
    REQUEST,
    RESPONSE
} TYPE;

/*command type*/
typedef struct {
    TYPE type; /*REQUEST or RESPONSE*/
    int number; /*the integer that is sent*/
    unsigned int from; /*the sender of the cmd*/
    unsigned int to; /*the receiver of the cmd*/
} cmd_t;

int main(int argc, char *argv[]) {

    RCCE_init(&argc, &argv);
    iRCCE_init();

    srand_core(); /*seed the rand()*/

    int numreqs; /*number of total msgs (REQ and RESP) to send*/
    if (argc == 1) {
        numreqs = 10000;
    }
    else {
        numreqs = atoi(argv[1]);
    }

    listen(numreqs);

    RCCE_finalize();
    return 0;
}

void listen(int numreqs) {

    int cores = RCCE_num_ues(); /*number of cores*/
    int core = RCCE_ue(); /*core id*/
    
    iRCCE_RECV_REQUEST *recv_requests; /*recvs for every other core*/
    iRCCE_RECV_REQUEST *recv_current; /*to be used with the waitlist test function*/
    iRCCE_SEND_REQUEST *send_current; /*to be used with the sendlist test function*/
    
    char buf[cores * 32]; /*buffer for incoming data*/
    int sender;

    /*init the waitlists*/
    iRCCE_init_wait_list(&waitlist);
    iRCCE_init_wait_list(&sendlist);

    recv_requests = (iRCCE_RECV_REQUEST *) calloc(cores, sizeof (iRCCE_RECV_REQUEST));
    if (recv_requests == NULL) {
        PRINTD("could not alloc recv_reqs");
        return;
    }

    /*Create recv request for each possible (other) core.*/
    for (sender = 0; sender < cores; sender++) {
        if (sender != core) {
            iRCCE_irecv(buf + sender * 32, 32, sender, &recv_requests[sender]);
            iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[sender]);
        }
    }

    RCCE_barrier(&RCCE_COMM_WORLD);
    PRINTD("started..");

    while (1) {
        /*if all sends are completed print a success message once*/
        if (scounter >= numreqs && print_success) {
            PRINTD("- %d -Sends completed! Have rcved %d msgs.", scounter, rcounter);
            print_success = 0;
        }

        /*if not enough messages have been sent && to_send (should send)*/
        if (snccounter < numreqs && to_send) {
            send(-1, -1);
            to_send = 0; /*wait for this request to be completed*/
        }

        /*test any send for completion*/
        iRCCE_test_any(&sendlist, &send_current, NULL);
        if (send_current != NULL) {
            free(send_current->privbuf);
            free(send_current);

            scounter++; /*++ send confirmed counter*/
        }

        /*test any recv for completion*/
        iRCCE_test_any(&waitlist, NULL, &recv_current);
        if (recv_current != NULL) {

            sender = recv_current->source; /*the sender of the message*/
            char *base = buf + sender * 32;

            cmd_t * cmd = (cmd_t *) base; /*retrieve the cmd received*/

            if (cmd->type == RESPONSE) {
                /*if the wrong number received - validation failed*/
                if (cmd->number != number_sent) {
                    PRINTD("[Expected] num: %5d from %02d, to %02d | [Received] num: %5d from %02d (%02d), to %02d",
                            number_sent, number_sent_to, RCCE_ue(), cmd->number, sender, cmd->from, cmd->to);
                }
                else {
                    to_send = 1; /*in order to send the next message*/
                }
#define CONT_ON_ERROR_
#ifdef CONT_ON_ERROR 
                to_send = 1;
#endif
            }
            else if (cmd->type == REQUEST) {
                send(sender, cmd->number); /*simply send back the received number*/
            }
            else {
                PRINTD("Received unknown cmd type %d", cmd->type);
            }

            /*make a new recv request for the sender*/
            iRCCE_irecv(base, 32, sender, &recv_requests[sender]);
            iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[sender]);

            rcounter++; /*++ received counter*/
        }
    }
}

void send(int core, int number) {

    cmd_t cmd;

    if (core < 0) { /*new REQUEST*/
        do {
            core = rand_range(RCCE_num_ues()) - 1;
        } while (core == RCCE_ue());

        cmd.type = REQUEST;
        cmd.number = ++number_sent;
        number_sent_to = core;
    } /*RESPONSE*/
    else {
        cmd.type = RESPONSE;
        cmd.number = number;
    }

    cmd.from = RCCE_ue();
    cmd.to = core;

    /*allocate the data buffer and the request that will be used for this send*/
    char *data = (char *) calloc(32, sizeof (char));
    iRCCE_SEND_REQUEST *s = (iRCCE_SEND_REQUEST *) malloc(sizeof (iRCCE_SEND_REQUEST));
    if (data == NULL || s == NULL) {
        PRINTD("malloc while sending failed");
        exit(-1);
    }

    memcpy(data, &cmd, sizeof (cmd_t)); /*cp the data to the data buffer*/
    if (iRCCE_isend(data, 32, core, s) != iRCCE_SUCCESS) {
        iRCCE_add_to_wait_list(&sendlist, s, NULL);
    }
    else { /*message delivered*/
        free(s);
        free(data);
        scounter++;
    }

    /*increase the "non-confirmed" counter*/
    snccounter++;
}

/*______________________________________________________________________________
 * Help functions
 *______________________________________________________________________________
 */

/* 
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 */
inline long rand_range(long r) {
    int m = RAND_MAX;
    long d, v = 0;

    do {
        d = (m > r ? r : m);
        v += 1 + (long) (d * ((double) rand() / ((double) (m) + 1.0)));
        r -= m;
    } while (r > 0);
    return v;
}

/*
 * Seeding the rand()
 */
inline void srand_core() {
    double timed_ = wtime();
    unsigned int timeprfx_ = (unsigned int) timed_;
    unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
    srand(time_ + (13 * (RCCE_ue() + 1)));
}
