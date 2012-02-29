/*
 * File:   iRCCE_recv_all.c
 * Author: trigonak
 *
 * Created on March 4, 2011, 5:45 PM
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "tm.h"
#include "task.h"

#define ME printf("[%d] ", RCCE_ue())
#define PRINTD(args...) ME; printf(args); printf("\n"); fflush(stdout)

enum BOOLEAN {
    FALSE,
    TRUE
};

#include "RCCE.h"
#include "iRCCE.h"
//#define PROBLEM

int rcounter = 0;
int scounter = 0;
int snccounter = 0;
//char data[32];
int res = 0;
int should_send = 1;


iRCCE_WAIT_LIST waitlist, sendlist;

void sendb(int);
void listen(void *);

void taskmain(int argc, char *argv[]) {

    RCCE_init(&argc, &argv);
    iRCCE_init();

    //RCCE_barrier(&RCCE_COMM_WORLD);

    taskcreate(listen, NULL, 30000);

    int msgs = atoi(argv[1]);

    int target = (RCCE_ue() + 1) % RCCE_num_ues();


    while (snccounter < msgs) {
        /* Push recieves & sends. */
        iRCCE_irecv_push();
        iRCCE_isend_push();

        taskudelay(10);
        if (should_send) {
            sendb(target);
            should_send = 0;
        }
        
        //PRINTD("sent: %d", snccounger);

        /*
                if (snccounger < 500) { //
                    double now = RCCE_wtime();
                    if (now - sent_last > wait_time) {
                        sent_last = now;
                        sendb(-1);
                        //sendb(-1);

                    }
                }
                else {
                    if (rcounter % 100 == 1)
                        printf("%d: Send %d\tReceived %d msgs\n", RCCE_ue(), scounter, rcounter);
                    fflush(stdout);
                }
         */
    }

    while (scounter < msgs) {
        taskdelay(11);
    }
    PRINTD("COMPLETED all %d msgs.", scounter);
    taskexit(0);
}

void listen(void *arg) {

    int cores = RCCE_num_ues();
    int core = RCCE_ue();
    iRCCE_RECV_REQUEST *recv_requests;
    iRCCE_RECV_REQUEST * recv_requests1[cores];
    iRCCE_RECV_REQUEST *recv_current;
    iRCCE_SEND_REQUEST *send_current;
    char buf[cores * 32];
    char *buf1[cores];
    int i;

    for (i = 0; i < cores; i++) {
        buf1[i] = (char *) malloc(32 * sizeof (char));
        if (buf1[i] == NULL) {
            PRINTD("could not alloc buf1");
        }
        recv_requests1[i] = (iRCCE_RECV_REQUEST *) malloc(sizeof (iRCCE_RECV_REQUEST));
        if (recv_requests1[i] == NULL) {
            PRINTD("could not alloc rev_requests1");
        }
    }


    recv_requests = (iRCCE_RECV_REQUEST*) calloc(cores, sizeof (iRCCE_RECV_REQUEST));
    if (recv_requests == NULL) {
        PRINTD("could not alloc recv_reqs");
        return;
    }

    iRCCE_init_wait_list(&waitlist);
    iRCCE_init_wait_list(&sendlist);



    // Create recv request for each possible (other) core.
    for (i = 0; i < cores; i++) {
        if (i != core) {
            //iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
            //iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[i]);
            //iRCCE_irecv(buf1[i], 32, i, recv_requests1[i]);
            //iRCCE_add_to_wait_list(&waitlist, NULL, recv_requests1[i]);
            // iRCCE_irecv(privbuf, 32, i, recv_requests1[i]);

            char *privbuf = (char *) malloc(32);

            iRCCE_RECV_REQUEST *r_req = (iRCCE_RECV_REQUEST *) malloc(sizeof (iRCCE_RECV_REQUEST));
            iRCCE_irecv(privbuf, 32, i, r_req);
            iRCCE_add_to_wait_list(&waitlist, NULL, r_req);
        }
    }



    //RCCE_barrier(&RCCE_COMM_WORLD);
    printf("started..");
    fflush(stdout);



    /* Wait for any to complete. */
    /*
        iRCCE_wait_any(&waitlist, &send_current, &recv_current);
     */
    while (1) {
        while (iRCCE_test_any(&waitlist, &send_current, &recv_current) != iRCCE_SUCCESS) {
            if (iRCCE_test_any(&sendlist, &send_current, &recv_current) == iRCCE_SUCCESS) {
                break;
            }
            taskndelay(123);
        }
        if (send_current != NULL) {
            scounter++;
            //PRINTD("%3d >>", scounter);
            free(send_current->privbuf);
            free(send_current);

            should_send = 1;
        }
        else if (recv_current != NULL) {
            // PRINTD("%3d << %d", rcounter, recv_current->source);

            /* the sender of the message*/
            i = recv_current->source;

            /* We have all info now, print! */
            //printf("%d: %s\n", core, buf + i * 32); fflush(stdout);

            free(recv_current->privbuf);
            free(recv_current);

            //buf1[i] = (char *) malloc(32 * sizeof(char));
            char *privbuf = (char *) malloc(32);
            iRCCE_RECV_REQUEST *r_req = (iRCCE_RECV_REQUEST *) malloc(sizeof (iRCCE_RECV_REQUEST));
            //recv_requests1[i] = (iRCCE_RECV_REQUEST *) malloc(sizeof(iRCCE_RECV_REQUEST));

            /* Create request for new message from this core, add to waitlist */
            //iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
            //iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[i]);
            //iRCCE_irecv(buf1[i], 32, i, recv_requests1[i]);
            //iRCCE_irecv(privbuf, 32, i, recv_requests1[i]);
            //iRCCE_add_to_wait_list(&waitlist, NULL, recv_requests1[i]);
            iRCCE_irecv(privbuf, 32, i, r_req);
            iRCCE_add_to_wait_list(&waitlist, NULL, r_req);

            rcounter++;
        }

        /* Push recieves & sends. */
        iRCCE_irecv_push();
        iRCCE_isend_push();
    }

}

void sendb(int core) {

    int target = core;

    /* Choose a random target, but not the core itself. */
    if (target < 0) {
        do {
            target = rand() % RCCE_num_ues();
        } while (target == RCCE_ue());
    }

    /* Push sends in queue. */
    iRCCE_isend_push();


    char *data = (char *) calloc(32, sizeof (char));
    iRCCE_SEND_REQUEST *s = (iRCCE_SEND_REQUEST *) malloc(sizeof (iRCCE_SEND_REQUEST));
    int *flag = (int *) malloc(sizeof (int));
    if (data == NULL || s == NULL || flag == NULL) {
        printf("malloc while sending failed");
    }

    /* Make nice message. */
    sprintf(data, "Message %d from %d to %d!", snccounter, RCCE_ue(), target);

    /* Issue send. */


    /* Push again. */
    /*
        iRCCE_isend_push();
        iRCCE_irecv_push();
     */


    //    iRCCE_isend_test(s, flag);
    if (iRCCE_isend(data, 32, target, s) != iRCCE_SUCCESS) {
        iRCCE_add_to_wait_list(&sendlist, s, NULL);
    }
    else {
        scounter++;
        // PRINTD("%3d >> ||", scounter);
        free(s);
        free(data);
    }

    free(flag);

    iRCCE_isend_push();
    iRCCE_irecv_push();

    /* Increase counter. */
    snccounter++;
}
