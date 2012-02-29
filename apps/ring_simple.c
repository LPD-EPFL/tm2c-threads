/* 
 * File:   iRCCE_ring.c
 * Author: trigonak
 *
 * Created on May 03, 2011, 5:47 PM
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

int main(int argc, char *argv[]) {

    RCCE_init(&argc, &argv);
    iRCCE_init();
    
    //pass as an argument the number of messages to send
    if (argc > 1) {
        nummsgs = atoi(argv[1]);
    }
    
    loop();
    
    RCCE_finalize();
    return 0;
}

void loop() {

    iRCCE_SEND_REQUEST s; //send request
    iRCCE_RECV_REQUEST recv_request; //recv request
    char buf[32], data[32]; //buffers for the incoming and outgoing messages
    int to_send = 1; //flag: to_send : if the node should send a new msg
    
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
            }
            else {
                to_send = 0; //w8 this request to be complete before sending a new one
            }
        }

        //if there is a pending send msg (!send), do iRCCE_isend_test
        if (!to_send && iRCCE_isend_test(&s, NULL) == iRCCE_SUCCESS) {
            scounter++;
            //PRINTD("Received: %s", buf);
            to_send = 1;
        }

        if (iRCCE_irecv_test(&recv_request, NULL) == iRCCE_SUCCESS) {
            rcounter++;
            iRCCE_irecv(buf, 32, RECEIVING_FROM, &recv_request);
        }
    }

    PRINTD("~~Finished sending %d and receiving %d msgs", scounter, rcounter);
}
