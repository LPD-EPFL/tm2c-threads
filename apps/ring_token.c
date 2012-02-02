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


long long int nummsgs = 1000, tokenval = 0;

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

    int ID = RCCE_ue();
    int RECEIVING_FROM = (ID + RCCE_num_ues() - 1) % RCCE_num_ues(); //previous node on the ring (incoming msgs)
    int SENDING_TO = (ID + 1) % RCCE_num_ues(); //next node on the ring (outgoing msgs)

    //init recv request
    iRCCE_irecv(buf, 32, RECEIVING_FROM, &recv_request);

    RCCE_barrier(&RCCE_COMM_WORLD);

    if (ID == 0) {
        PRINTD("Started. The token will be resubmitted %lld times!", nummsgs);
        memcpy(data, &tokenval, sizeof (long long int));
        iRCCE_isend(data, 32, SENDING_TO, NULL);
    }

    long long int printy = 1000;
    
    while (1) {
        if (iRCCE_irecv_test(&recv_request, NULL) == iRCCE_SUCCESS) {
            tokenval = *(int *) buf;
            if ((!ID) && tokenval > printy) {
                PRINTD("Token here: %lld", tokenval);
                printy = 2 * tokenval;
            }
            
            tokenval++;
            if (tokenval >= nummsgs) {
                PRINTD("~~Completed. Token here: %lld", tokenval);
                exit(0);
            }
            iRCCE_irecv(buf, 32, RECEIVING_FROM, &recv_request);
            memcpy(data, &tokenval, sizeof (long long int));
            iRCCE_isend(data, 32, SENDING_TO, &s);
        }
        iRCCE_isend_push();
    }
}