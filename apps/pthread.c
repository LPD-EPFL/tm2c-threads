#include "RCCE.h"
#include "iRCCE.h"
#include <pthread.h>

#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "../include/pubSubTM.h"


void *run(void *);
void listen(void);
iRCCE_SEND_REQUEST send(int);

int rcounter = 0;
int scounter = 0;
char data[32];

iRCCE_WAIT_LIST waitlist, sendlist;

int RCCE_APP(int argc, char **argv) {

    RCCE_init(&argc, &argv);
    int ID = RCCE_ue();

    pthread_t pt;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int rc = pthread_create(&pt, &attr, run, (void *) ID);
    if (rc) {
        printf("%d: could not start thread\n", ID);
    }
    else {
        printf("%d: started thread\n", ID);
    }
    

    long long int i = 0;
    while (i++ < 123456) {
        usleep(400000);
        TS; PRINTD("I am running in parallel with my thready!");
        fflush(stdout);
    }

    pthread_attr_destroy(&attr);
    pthread_join(pt, NULL);

    RCCE_finalize();
    printf("%d: joined thread\n", ID);
    pthread_exit(NULL);
    //return (0);
}

void *run(void *data) {
    iRCCE_init();

    listen();

    pthread_exit(NULL);
}

void listen() {

    int cores = RCCE_num_ues();
    int core = RCCE_ue();
    iRCCE_RECV_REQUEST *recv_requests;
    iRCCE_RECV_REQUEST *recv_current;
    iRCCE_SEND_REQUEST *send_current;
    char buf[cores * 32];
    int i;

    recv_requests = (iRCCE_RECV_REQUEST*) calloc(cores, sizeof (iRCCE_RECV_REQUEST));

    iRCCE_init_wait_list(&waitlist);
    iRCCE_init_wait_list(&sendlist);


    // Create recv request for each possible (other) core.
    for (i = 0; i < cores; i++) {
        if (i != core) {
            iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
            iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[i]);
        }
    }

    double sent_last = RCCE_wtime();

    while (1) {
        /* Push recieves & sends. */
        iRCCE_irecv_push();
        iRCCE_isend_push();

        double now = RCCE_wtime();
        if (now - sent_last > 0.3) {
            sent_last = now;
            iRCCE_SEND_REQUEST s = send(-1);
            iRCCE_add_to_wait_list(&waitlist, &s, NULL);
        }

        /* Wait for any to complete. */
        //iRCCE_wait_any(&waitlist, &dummy, &cur);
        iRCCE_test_any(&waitlist, &send_current, &recv_current);
        if (send_current != NULL) {
/*
            TS; PRINTD(">> %d", send_current->dest);
*/
        } else if (recv_current != NULL) {
            ///TS; PRINTD("<< %d", recv_current->source);

            /* the sender of the message*/
            i = recv_current->source;

            /* We have all info now, print! */


/*
            printf("%d: %s\n", i, buf + i * 32);
*/


            char *base = buf + i * 32;
            char oper = (char) *(base);
            void *ptr =  (void *) strtoul(base + 1, NULL, 0);
/*
            printf("%d: %d - %p\n", i, (int) oper, ptr);
*/

            /* Create request for new message from this core, add to waitlist */
            iRCCE_irecv(buf + i * 32, 32, i, &recv_requests[i]);
            iRCCE_add_to_wait_list(&waitlist, NULL, &recv_requests[i]);

            rcounter++;
        }

    }
}

iRCCE_SEND_REQUEST send(int core) {
    iRCCE_SEND_REQUEST s;
    int target = core;
    char oper;

    /* Choose a random target, but not the core itself. */
    if (target < 0) {
        do {
            long long int rand1 = rand();
            target =  rand1 % RCCE_num_ues();
            oper = (char) (rand1 % PS_NUM_OPS);

        } while (target == RCCE_ue());
    }

    /* Push sends in queue. */
    iRCCE_isend_push();
    iRCCE_irecv_push();

/*
    *data = oper;
    void *ptr = &s;
    *(data + 1) = (char *) ptr;
*/
    /* Make nice message. */
    //sprintf(data, "Message %d from %d to %d!", scounter, RCCE_ue(), target);

    void *g = malloc(sizeof(int));
/*
    PRINTD("alloc - %p", g);
*/
    sprintf(data, "%c%p", oper, g);
    RCCE_shfree(g);

    /* Issue send. */
    iRCCE_isend(data, 32, target, &s);
    //iRCCE_add_to_wait_list(&sendlist, &s, NULL);

    /* Push again. */
    iRCCE_isend_push();
    iRCCE_irecv_push();

/*
    iRCCE_isend_wait(&s);
*/

    /* Increase counter. */
    scounter++;

    return s;
}
