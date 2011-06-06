/*
 * File:   iRCCE_all.c
 * Author: Vasileios Trigonakis
 *
 * Created on May 30, 2011, 5:45 PM
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "iRCCE.h"


#define ME printf("[%02d] ", RCCE_ue())
#define PRINTD(args...) {ME; printf(args); printf("\n"); fflush(stdout);}


void listen(int repeats);
void send(int, int);
inline long rand_range(long r);
inline void srand_core();


int rcounter = 0; /*receives counter  */
int scounter = 0; /*sends counter*/
int snccounter = 0; /*sends including the ones queued*/
int print_success = 1; /*used to print the success message only once*/
unsigned int number_sent = 0, number_sent_to = -1, to_send = 1;
iRCCE_WAIT_LIST waitlist, sendlist; /*incoming & outgoing msgs waitlists*/

typedef enum {
    REQUEST,
    RESPONSE
} TYPE;

typedef struct {
    TYPE type;
    int number;
    unsigned int from;
    unsigned int to;
} cmd_t;

int main(int argc, char *argv[]) {

    RCCE_init(&argc, &argv);
    iRCCE_init();

    srand_core(); /*seed the rand()*/

    int repeats; /*number of messages to send*/
    if (argc == 1) {
        repeats = 10000;
    }
    else {
        repeats = atoi(argv[1]);
    }

    listen(repeats);

    return 0;
}

void listen(int repeats) {

    int cores = RCCE_num_ues();
    int core = RCCE_ue();
    iRCCE_RECV_REQUEST *recv_requests, *recv_current;
    iRCCE_SEND_REQUEST *send_current;
    char buf[cores * 32];
    int sender;

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
        /*if all sends are completed*/
        if (scounter >= repeats && print_success) {
            PRINTD("- %d -Sends completed! Have rcved %d msgs.", scounter, rcounter);
            print_success = 0;
        }

        /*if not enough messages are sent yet & to_send*/
        if (snccounter < repeats && to_send) {
            send(-1, -1);
            to_send = 0;
        }

        /*test any send for completion*/
        iRCCE_test_any(&sendlist, &send_current, NULL);
        if (send_current != NULL) {
            cmd_t * cmd = (cmd_t *) send_current->privbuf;

            PRINTD("-->>[%s] to %02d (%d) for %d (%d)", cmd->type == REQUEST ? "REQUEST " : "RESPONSE", 
                cmd->to, number_sent_to, cmd->number, number_sent);


            free(send_current->privbuf);
            free(send_current);

            scounter++; /*++ send confirmed counter*/
        }

        /*test any recv for completion*/
        iRCCE_test_any(&waitlist, NULL, &recv_current);
        if (recv_current != NULL) {

            /*the sender of the message*/
            sender = recv_current->source;
            char *base = buf + sender * 32;
            /*the cmd sent*/
            cmd_t * cmd = (cmd_t *) base;

            PRINTD("<<--[%s] to %02d for %d", cmd->type == REQUEST ? "REQUEST " : "RESPONSE", 
                cmd->to, cmd->number);
            
            if (cmd->type == RESPONSE) {
                /*if the wrong number received*/
                if (cmd->number != number_sent) {
                    PRINTD("\t\t\t\t\t[Expected] num: %5d from %02d, to %02d | [Received] num: %5d from %02d (%02d), to %02d",
                            number_sent, number_sent_to, RCCE_ue(), cmd->number, sender, cmd->from, cmd->to);
                }
                else {
                    to_send = 1; /*in order to send the next message*/
                }
            }
            else if (cmd->type == REQUEST) { /**/
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

    if (core < 0) {
        do {
            core = rand_range(RCCE_num_ues()) - 1;
        } while (core == RCCE_ue());
    }

    if (number < 0) {
        cmd.type = REQUEST;
        cmd.number = ++number_sent;
        number_sent_to = core;
    }
    else {
        cmd.type = RESPONSE;
        cmd.number = number;
    }

    cmd.from = RCCE_ue();
    cmd.to = core;

    /*
        PRINTD("[%s] to %02d for %d", cmd.type == REQUEST ? "REQUEST " : "RESPONSE", cmd.to, cmd.number);
     */

    /*allocate the data buffer and the request that will be used for this send*/
    char *data = (char *) calloc(32, sizeof (char));
    iRCCE_SEND_REQUEST *s = (iRCCE_SEND_REQUEST *) malloc(sizeof (iRCCE_SEND_REQUEST));
    if (data == NULL || s == NULL) {
        PRINTD("malloc while sending failed");
        exit(-1);
    }

    memcpy(data, &cmd, sizeof (cmd_t));

    if (iRCCE_isend(data, 32, core, s) != iRCCE_SUCCESS) {
        iRCCE_add_to_wait_list(&sendlist, s, NULL);
    }
    else {
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
    double timed_ = RCCE_wtime();
    unsigned int timeprfx_ = (unsigned int) timed_;
    unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
    srand(time_ + (13 * (RCCE_ue() + 1)));
}
