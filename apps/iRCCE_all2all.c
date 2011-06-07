/*
 * File:   iRCCE_all2all.c
 * Author: Vasileios Trigonakis
 *
 * Created on April 14, 2011, 5:45 PM
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define ME printf("[%d] ", RCCE_ue())
#define PRINTD(args...) {ME; printf(args); printf("\n"); fflush(stdout);}

#include "RCCE.h"
#include "iRCCE.h"

int rcounter = 0; /*receives counter  */
int scounter = 0; /*sends counter*/
int snccounter = 0; /*sends including the ones queued*/
int print_success = 1; /*used to print the success message only once*/


iRCCE_WAIT_LIST waitlist, sendlist;

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

void send();
void listen(int repeats);

int main(int argc, char *argv[]) {

    RCCE_init(&argc, &argv);
    iRCCE_init();
    
    srand_core();

    int repeats; /*number of messages to send*/
    if (argc == 1) {
        repeats = 1000;
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
    int i;

    iRCCE_init_wait_list(&waitlist);
    iRCCE_init_wait_list(&sendlist);

    recv_requests = (iRCCE_RECV_REQUEST *) calloc(cores, sizeof (iRCCE_RECV_REQUEST));
    if (recv_requests == NULL) {
        PRINTD("could not alloc recv_reqs");
        return;
    }

    /*Create recv request for each possible (other) core.*/
    for (i = 0; i < cores; i++) {
        if (i != core) {
            iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
            iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[i]);
        }
    }
    
    RCCE_barrier(&RCCE_COMM_WORLD);
    PRINTD("started..");

    int debug = 0;
    double time_last_send = RCCE_wtime();

    while (1) {
        /*if all sends are completed*/
        if (scounter >= repeats && print_success) {
            PRINTD("- %d -Sends completed! Hace rcved %d msgs.", scounter, rcounter);
            print_success = 0;
            debug = 0;
        }

        /*if not enough messages are sent yet*/
        //if (snccounter < repeats) {
        if (snccounter < repeats) {
            debug = 1;
            send();
        }

        /*test any send for completion*/
        iRCCE_test_any(&sendlist, &send_current, &recv_current);
        if (send_current != NULL) {
            free(send_current->privbuf);
            free(send_current);

            scounter++;
            time_last_send = RCCE_wtime();
        }

        /*test any recv for completion*/
        iRCCE_test_any(&waitlist, &send_current, &recv_current);
        if (recv_current != NULL) {

            /*the sender of the message*/
            i = recv_current->source;

            /*print the received msg*/
            //PRINTD("%s", recv_current->privbuf);

            /*make a new recv request for the sender*/
            iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
            iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[i]);

            rcounter++;
        }
        
        if (RCCE_wtime() - time_last_send > 2.0 && debug) {
            PRINTD("STUCK?? Sent: %d, Sent confirmed: %d, Received: %d\n" \
                        "\tWaitlist length: %d", snccounter, scounter, rcounter, iRCCE_wait_list_length(&sendlist));
            iRCCE_print_sendlist(&sendlist, RCCE_ue());

            time_last_send = RCCE_wtime();
            debug = 0;
        }
    }
}

void send() {

    int core = -1;
    do {
        core = rand_range(RCCE_num_ues()) - 1;
    } while (core == RCCE_ue());
    
    //printf("(%d)", core);
    
    /*allocate the data buffer and the request that will be used for this send*/
    char *data = (char *) calloc(32, sizeof (char));
    iRCCE_SEND_REQUEST *s = (iRCCE_SEND_REQUEST *) malloc(sizeof (iRCCE_SEND_REQUEST));
    if (data == NULL || s == NULL) {
        PRINTD("malloc while sending failed");
        exit(-1);
    }

    /*create the message*/
    sprintf(data, "Message %d from %d to %d!", snccounter, RCCE_ue(), core);


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