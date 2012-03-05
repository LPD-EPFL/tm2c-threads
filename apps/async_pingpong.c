/* 
 * File:   iRCCE_async_pingpong.c
 * Author: trigonak
 *
 * Created on March 4, 2011, 5:45 PM
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "RCCE.h"
#include "iRCCE.h"

#define DATA 128
#define M printf("%d: ", ME)

int main(int argc, char **argv) {
   
    RCCE_init(&argc, &argv);
    iRCCE_init();

    iRCCE_SEND_REQUEST send_req;
    iRCCE_RECV_REQUEST recv_req;
    
    int ME = RCCE_ue();
    int OTHER = !ME;

    const char *s = "this is a message! like it?";
    unsigned int size = strlen(s) + 1;
    
    char sbuffer[size];
    strcpy(sbuffer, "this is a message! like it?");

    char rbuffer[size];

    int i = 0;
    while(i < size) {
        long int seed1 = (long int) (wtime()*1000);
        srand((seed1 % 128) * 7);
        long int time = rand() % 1000;
        M; printf("1.sleeps for %ld usec\n", time);
        usleep(time);

        int send = iRCCE_isend((sbuffer + i), sizeof(char), OTHER, &send_req);

        seed1 = (long int) (wtime()*1000);
        srand((seed1 % 128) * 7);
        time = rand() % 1000;
        M; printf("2.sleeps for %ld usec\n", time);
        usleep(time);

        int recv = iRCCE_irecv((rbuffer + i), sizeof(char), OTHER, &recv_req);
        iRCCE_isend_push();
        if (recv != 0) {
            iRCCE_irecv_wait(&recv_req);
        }
        M; printf("3.received %c\n", *(rbuffer + i));

        if(send != 0) {
            iRCCE_isend_wait(&send_req);
        }
        
        M; printf("4.next round (%d)\n", ++i);
    }



    RCCE_finalize();

    return 0;
}

