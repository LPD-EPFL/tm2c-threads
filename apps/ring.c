/* 
 * File:   iRCCE_ring.c
 * Author: trigonak
 *
 * Created on April 17, 2011, 3:47 PM
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "iRCCE_lib.h"

#define ME printf("[%d] ", RCCE_ue())
#define PRINTD(args...) {ME; printf(args); printf("\n"); fflush(stdout);}

int rcounter = 0; //receives counter
int scounter = 0; //confirmed (completed) sends counter
int snccounter = 0; //non-confirmed (completed) sends counter
int nummsgs = 100; //number of messages each node will send to the neighbor 

void loop();
void print_send_req(iRCCE_SEND_REQUEST *s); //prints a send request
void print_recv_req(iRCCE_RECV_REQUEST *s); //prints a recv request

int main(int argc, char *argv[]) {

    RCCE_init(&argc, &argv);
    iRCCE_init();
    
    //pass as an argument the number of messages to send
    if (argc > 1) {
        nummsgs = atoi(argv[1]);
    }
    
    loop();
    return 0;
}

void loop() {

    iRCCE_SEND_REQUEST s; //send request
    iRCCE_RECV_REQUEST recv_request, previous_request; //current and previous recv requests
    char buf[32], data[32]; //buffers for the incoming and outgoing messages
    double recv_time; //the time the node last received a msg
    double send_time; //the time the node last sent a msg
    /* flags:
     * print_r : if a debug msg should be print in case the node did not receive a msg for 2 seconds
     * print_s : if a debug msg should be print in case the node did not completed sending a msg for 2 seconds
     * to_send : if the node should send a new msg
     */
    int print_r = 0, print_s = 0, to_send = 1;
    
    int RECEIVING_FROM = (RCCE_ue() + RCCE_num_ues() - 1) % RCCE_num_ues(); //previous node on the ring (incoming msgs)
    int SENDING_TO = (RCCE_ue() + 1) % RCCE_num_ues(); //next node on the ring (outgoing msgs)

    //init recv request
    iRCCE_irecv(buf, 32, RECEIVING_FROM, &recv_request);
    
    RCCE_barrier(&RCCE_COMM_WORLD);

    PRINTD("Started. I will send to %d and recv from %d %d messages", SENDING_TO, RECEIVING_FROM, nummsgs);
    

    //until #nummsgs sends and recvs have completed 
    while (rcounter < nummsgs || scounter < nummsgs) {
        //if previus msg delivered and have sent < #nummsgs msgs
        if (to_send && scounter < nummsgs) {
            snccounter++;
            sprintf(data, "Message %d from %d to %d!", snccounter, RCCE_ue(), SENDING_TO);
            if (iRCCE_isend(data, 32, SENDING_TO, &s) == iRCCE_SUCCESS) {
                scounter++;
                print_s = 0;
            }
            else {
                print_s = 1;
                to_send = 0; //w8 this request to be complete before sending a new one
            }
            send_time = RCCE_wtime();
        }

        //if there is a pending send msg (!send), do iRCCE_isend_test
        if (!to_send && iRCCE_isend_test(&s, NULL) == iRCCE_SUCCESS) {
            scounter++;
            print_s = 0;
            to_send = 1;
        }
        //if there is a outgoing request not delivered for ~2 seconds
        else if (print_s && RCCE_wtime() - send_time > 2) {
            ME; printf("|| NO SEND 2s> SNC: %d, SC: %d, RC: %d\n\tLast sent msg: %s\n", snccounter, scounter, rcounter, data);
            print_send_req(&s); //print the details of the request
            print_s = 0;
        }

        if (iRCCE_irecv_test(&recv_request, NULL) == iRCCE_SUCCESS) {
            rcounter++;

            //keep a copy of the previous recv request for printing
            memcpy(&previous_request, &recv_request, sizeof(iRCCE_RECV_REQUEST));
            iRCCE_irecv(buf, 32, RECEIVING_FROM, &recv_request);
            recv_time = RCCE_wtime();
            print_r = 1;
        }
        /*if there are still msgs to be delivered (rcounter < nummsgs) but no
         msg was delivered the last 2 seconds*/
        else if (print_r && RCCE_wtime() - recv_time > 2 && rcounter < nummsgs) {
            ME; printf("|| NO RECV 2s> RC: %d\n\tMsg in the incoming buffer: %s\n", rcounter, buf);
            printf("|  Previous recv request (recv# %d)\n", rcounter - 1);
            print_recv_req(&previous_request); //print the previous successful request
            printf("|  Current recv request (recv# %d)\n", rcounter);
            print_recv_req(&recv_request); //print the pending request

            print_r = 0;
        }

    }

    PRINTD("~~Finished sending %d and receiving %d msgs", scounter, rcounter);

}

#define PS(what, how) printf("|  " #what " = " #how "\n", what)

void print_send_req(iRCCE_SEND_REQUEST *s) {
    ME;
    printf("__________________________________________________________________________________________\n");
    printf("|| iRCCE_SEND_REQUEST -- Sent: %d | Sent confirmed: %d | Recved: %d", snccounter, scounter, rcounter);
    PS(s->combuf, % p);
    PS(s->dest, % d);
    PS(s->finished, % d);
    PS(s->label, % d);
    PS(s->next, % p);
    printf("|  s->ready = %d\n", RCCE_probe(*s->ready));
    printf("|  s->sent = %d\n", RCCE_probe(*s->sent));
    PS(s->size, % d);
    PS(s->nbytes, % d);
    PS(s->chunk, % d);
    PS(s->remainder, % d);

    fflush(stdout);
}

void print_recv_req(iRCCE_RECV_REQUEST *s) {
    ME;
    printf("__________________________________________________________________________________________\n");
    printf("|| iRCCE_RECV_REQUEST -- Recved: %d | Sent: %d | Sent confirmed: %d", rcounter, snccounter, scounter);
    PS(s->combuf, % p);
    PS(s->privbuf, % p);
    PS(s->source, % d);
    PS(s->finished, % d);
    PS(s->label, % d);
    PS(s->next, % p);
    printf("|  s->ready = %d\n", RCCE_probe(*s->ready));
    printf("|  s->sent = %d\n", RCCE_probe(*s->sent));
    PS(s->started, % d);
    PS(s->size, % d);
    PS(s->nbytes, % d);
    PS(s->chunk, % d);
    PS(s->remainder, % d);

    fflush(stdout);
}
