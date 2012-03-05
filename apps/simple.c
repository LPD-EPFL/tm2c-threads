/* 
 * File:   iRCCE_simple.c
 * Author: trigonak
 *
 * Created on April 15, 2011, 2:49 PM
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "task.h"

#define ME printf("[%d] ", RCCE_ue())
#define PRINTD(args...) {ME; printf(args); printf("\n"); fflush(stdout);}

#include "RCCE.h"
#include "iRCCE.h"
#include "iRCCE_lib.h"
//#define PROBLEM

int rcounter = 0;
int scounter = 0;
int print = 1;
int nummsgs;

void sendb(int);
void listen(void *);
void print_send_req(iRCCE_SEND_REQUEST *s);
void print_recv_req(iRCCE_RECV_REQUEST *s);

void taskmain(int argc, char *argv[]) {

    RCCE_init(&argc, &argv);
    iRCCE_init();

    RCCE_barrier(&RCCE_COMM_WORLD);

    taskcreate(listen, NULL, 30000);

    nummsgs = atoi(argv[1]);

    int target = (RCCE_ue() + 1) % RCCE_num_ues();


    while (scounter < nummsgs) {
        taskudelay(10);
        sendb(target);
    }

    while (rcounter < nummsgs) {
        taskdelay(11);
    }
    PRINTD("COMPLETED all %d msgs.", rcounter);
    taskexit(0);
}

void listen(void *arg) {

    int cores = RCCE_num_ues();
    int core = RCCE_ue();
    iRCCE_RECV_REQUEST *recv_requests, recv_request;
    char buf[cores * 32];
    char buf1[32];
    int i;
    int recv = (RCCE_ue() + RCCE_num_ues() - 1) % RCCE_num_ues();

    /*recv_requests = (iRCCE_RECV_REQUEST *) calloc(cores, sizeof (iRCCE_RECV_REQUEST));
    if (recv_requests == NULL) {
        PRINTD("could not alloc recv_reqs");
        return;
    }

    // Create recv request for each possible (other) core.
    
        for (i = 0; i < cores; i++) {
            if (i != core) {
                iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
            }
        }
     */

    iRCCE_irecv(buf1, 32, recv, &recv_request);
    PRINTD("started..");

    double recv_time = RCCE_ue();
    while (rcounter < nummsgs) {
        /*
                for (i = 0; i < cores; i++) {
                    if (i != core) {
                        if (iRCCE_irecv_test(&recv_requests[i], NULL) == iRCCE_SUCCESS) {
                            rcounter++;
                            iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
                        }
                    }
                }
         */
        if (iRCCE_irecv_test(&recv_request, NULL) == iRCCE_SUCCESS) {
            rcounter++;
            iRCCE_irecv(buf1, 32, recv, &recv_request);
            recv_time = wtime();
        }
        else if (wtime() - recv_time > 4.0) {
            PRINTD("No Recv for 4 secs.");
            print_recv_req(&recv_request);
            recv_time *= 2;
            /*
                        if (iRCCE_irecv_cancel(&recv_request, NULL) == iRCCE_SUCCESS) {
                            PRINTD("Cancelled and created new recv.");
                            iRCCE_irecv(buf1, 32, recv, &recv_request);
                            recv_time = wtime();
                        }
             */
        }
        taskndelay(10); //TODO: is taskyield enough?

    }

    taskexit(0);


}

void sendb(int target) {

    //char *data = (char *) calloc(32, sizeof (char));
    char data[32];
    //iRCCE_SEND_REQUEST *s = (iRCCE_SEND_REQUEST *) malloc(sizeof (iRCCE_SEND_REQUEST));
    /*
        if (data == NULL || s == NULL) {
            PRINTD("malloc while sending failed");
        }
     */
    iRCCE_SEND_REQUEST s;

    /* Make nice message. */
    sprintf(data, "Message %d from %d to %d!", scounter, RCCE_ue(), target);

    /*
        PRINTD("sending");
     */

    double started_sending = wtime();
    iRCCE_isend(data, 32, target, &s);
    while (iRCCE_isend_test(&s, NULL) != iRCCE_SUCCESS) {
        taskndelay(10);
        if (wtime() - started_sending > 2.0 && print) {
            PRINTD("STUCK! In msg %d. Have received: %d", scounter, rcounter);
            print_send_req(&s);
            print = 0;
        }
    }
    /*
        PRINTD("send");
     */
    scounter++;
    /*
        free(s);
        free(data);
     */
}

#define PS(what, how) printf("|  " #what " = " #how "\n", what)

void print_send_req(iRCCE_SEND_REQUEST *s) {
    PRINTD("|| iRCCE_SEND_REQUEST --");
    PS(s->combuf, % p);
    PS(s->nbytes, % d);
    PS(s->chunk, % d);
    PS(s->dest, % d);
    PS(s->finished, % d);
    PS(s->label, % d);
    PS(s->next, % p);
    printf("|  s->ready = %d\n", RCCE_probe(*s->ready));
    printf("|  s->sent = %d\n", RCCE_probe(*s->sent));
    PS(s->size, % d);

    fflush(stdout);
}

void print_recv_req(iRCCE_RECV_REQUEST *s) {
    PRINTD("|| iRCCE_RECV_REQUEST --");
    PS(s->combuf, % p);
    PS(s->privbuf, % p);
    PS(s->nbytes, % d);
    PS(s->chunk, % d);
    PS(s->source, % d);
    PS(s->finished, % d);
    PS(s->label, % d);
    PS(s->next, % p);
    printf("|  s->ready = %d\n", RCCE_probe(*s->ready));
    printf("|  s->sent = %d\n", RCCE_probe(*s->sent));
    PS(s->started, % d);
    PS(s->size, % d);

    fflush(stdout);
}
