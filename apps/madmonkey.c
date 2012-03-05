
/* 
 * Copyright 2010 Jacek Galowicz, Chair for Operating Systems,
 *                                RWTH Aachen University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <string.h>
#include <stdio.h>

#include "common.h"

#include "iRCCE.h"

#define MASTER 0
int num_ranks, remote_rank, my_rank;
iRCCE_WAIT_LIST general_waitlist;

void distribute_word();
void receive_word();
int  wait_for_first_finisher();
void random_string_work();
void tell_the_others_to_stop();
void send_stats();
void receive_stats();

// MASTER vars
char *orig_word;
double *timers;
unsigned long long int *tries;
double time_to_success;

// SLAVE vars
char *word_copy;
double time_total;
unsigned long long int tries_total;

iRCCE_RECV_REQUEST *recv_requests;
iRCCE_SEND_REQUEST *send_requests;

//
//       THIS CODE DOES _NOT_ WORK WITH THE OPENMP RCCE EMULATOR!
//
#ifdef _OPENMP
#warning THIS CODE DOES _NOT_ WORK WITH THE OPENMP RCCE EMULATOR!
#endif
//
//

MAIN(int argc, char **argv)
{
	int i, n;
	char * ptr;

	RCCE_init(&argc, &argv);
	iRCCE_init();

	iRCCE_init_wait_list(&general_waitlist);

	my_rank   = RCCE_ue();
	num_ranks = RCCE_num_ues();

	if(num_ranks == 1) {
		fprintf(stderr, "Madmonkey needs at least two UEs; try again\n");
		exit(-1);
	}

	if(argc < 2) {
		fprintf(stderr, "Madmonkey needs a word to be found as program argument; try again\n");
		fprintf(stderr, "(e.g. \"iRCCE\" should not take much longer than a minute to be found by one slave)\n");
		exit(-1);
	}

	for(ptr = argv[1]; (*ptr); ptr++) {
	  (*ptr) = tolower(*ptr);
	} 

	/* synchronize before starting: */
	RCCE_barrier(&RCCE_COMM_WORLD);

	if (my_rank == MASTER) {
		printf("Search for \"%s\" started!\n", argv[1]);
		
		orig_word = argv[1];

		distribute_word(orig_word);
		RCCE_barrier(&RCCE_COMM_WORLD);

		n = wait_for_first_finisher();
		tell_the_others_to_stop();
		RCCE_barrier(&RCCE_COMM_WORLD);

		receive_stats();

		unsigned long long int try_sum = 0;
		for(i=1; i<num_ranks; i++)
			try_sum += tries[i];

		printf("Landed a hit after %.3f seconds.\n", time_to_success);
		printf("Winner was rank %d.\n", n);
		printf("The %d Slaves needed %lld tries in total.\n", num_ranks-1, try_sum);

	}
	else {
		receive_word();
		RCCE_barrier(&RCCE_COMM_WORLD);

		random_string_work();
		RCCE_barrier(&RCCE_COMM_WORLD);

		send_stats();
	}

	RCCE_finalize();

	return 0;
}

void distribute_word(char * word)
{
	int len = strlen(word);
	int i;

	// Send to all ranks.
	// They've got it already because the program's
	// started with the same arguments, but we 
	// ignore that.
	for(i=1; i < num_ranks; i++) {
		iRCCE_send((char*)&len, sizeof(int), i);
		iRCCE_send(word, len, i);
	}
}

void receive_word()
{
	int len;

	// Receive stringlen, malloc buffer and receive string
	iRCCE_recv((char*)&len, sizeof(int), MASTER);
	word_copy = (char*)malloc(len*sizeof(char));
	iRCCE_recv(word_copy, len, MASTER);
}

int wait_for_first_finisher()
{
	int done, i, test;
	iRCCE_RECV_REQUEST* finisher_request;

	timers = (double*)malloc(num_ranks*sizeof(double));
	recv_requests = (iRCCE_RECV_REQUEST*)malloc(num_ranks*sizeof(iRCCE_RECV_REQUEST));

	for (i=1; i < num_ranks; i++) {
		iRCCE_irecv((char*)&timers[i], sizeof(double), i, &recv_requests[i]);
		iRCCE_add_to_wait_list(&general_waitlist, NULL, &recv_requests[i]);
	}

#if 0
	// Just wait for the first rank who gets the word
	done = 0;
	while(!done) {
		for (i=1; i < num_ranks; i++) {
			if (iRCCE_irecv_test(&recv_requests[i], &test) == iRCCE_SUCCESS) {
				done = 1;
				time_to_success = timers[i];
				break;
			}
		}
	}

	// Do not free recv requests, yet, since we will wait
	// for the other done-signals later.
	//free(recv_requests);

	return i;
#else
	// Use iRCCE_wait_any() function:
	iRCCE_wait_any(&general_waitlist, NULL, &finisher_request);	
	
	time_to_success = timers[finisher_request->source];
      
	return finisher_request->source;
#endif
}

void random_string_work()
{
	int signal = 0;
	unsigned long long int tries = 0;
	int n, done;
	char * randstr;
	double timer;

	iRCCE_RECV_REQUEST sig_request;
	iRCCE_SEND_REQUEST stopsig_request;

	iRCCE_irecv((char*)&signal, sizeof(int), MASTER, &sig_request);
	srand(time(NULL)*my_rank*(my_rank+20));

	randstr = (char*)malloc(strlen(word_copy)*sizeof(char));

	//printf("Starting to generate.\n");
	timer = wtime();
	timer = wtime();

	done = 0;
	while (!done) {
		// Generate a random string
		// only in the range from a-z
		for (n=0; n < strlen(word_copy); n++) {
			randstr[n] = (char)(rand() 
					* (double)26.0 / RAND_MAX 
					+0x61);
		}

		// Compare with the original
		for (n=0; n < strlen(word_copy); n++) {
			if (randstr[n] != word_copy[n]) {
				done = 0;
				break;
			}
			done = 1;
		}
		tries++;

		// Stop, if server says so.
		if (iRCCE_irecv_test(&sig_request, &n) == iRCCE_SUCCESS) {
			//printf("Slave got signal %d\n", signal);
			break;
		}
	}

	timer = wtime() -timer;


	if (signal == 0) {
		iRCCE_SEND_REQUEST timer_send_request;
		iRCCE_isend((char*)&timer, sizeof(double), MASTER, &timer_send_request);

		// Wait for the stop-signal the master sends to everyone
		while (iRCCE_isend_test(&timer_send_request, &n) != iRCCE_SUCCESS || RCCE_irecv_test(&sig_request, &n) != RCCE_SUCCESS);
		//printf("Got it myself and received stop signal.\n");
	}
	else {
		// Send message saying that we are ready, since the master
		// cannot abort his nonblocking receives.
		//printf("Slave done. (Was told to stop after %lld tries)\n", tries);	
		iRCCE_send((char*)&timer, sizeof(double), MASTER);
	}

	//printf("Slave done. (Got the word: \"%s\" after %d tries)\n", word_copy, tries);

	// Store time and tries to global vars
	time_total = timer;
	tries_total = tries;

	free(randstr);
}

void tell_the_others_to_stop()
{
	int i, done, test;
	int stop_signal = -1;

	send_requests = (iRCCE_SEND_REQUEST*)malloc(num_ranks*sizeof(iRCCE_SEND_REQUEST));

	for (i=1; i < num_ranks; i++) {
		iRCCE_isend((char*)&stop_signal, sizeof(int), i, &send_requests[i]);
		iRCCE_add_to_wait_list(&general_waitlist, &send_requests[i], NULL);
	}

	iRCCE_wait_all(&general_waitlist);

	free(send_requests);
	free(recv_requests);
}

void send_stats()
{
	// Send time and number of tries
	iRCCE_send((char*)&time_total, sizeof(double), MASTER);
	iRCCE_send((char*)&tries_total, sizeof(unsigned long long int), MASTER);
}

void receive_stats()
{
	int i, done, test;

	// This recv-calls could be done with blocking functions as well, of course.
	tries = (unsigned long long int*)malloc(num_ranks*sizeof(unsigned long long int));
	recv_requests = (iRCCE_RECV_REQUEST*)malloc(num_ranks*sizeof(iRCCE_RECV_REQUEST));

	for (i=1; i < num_ranks; i++) {
		iRCCE_irecv((char*)&timers[i], sizeof(double), i, &recv_requests[i]);
		iRCCE_add_to_wait_list(&general_waitlist, NULL, &recv_requests[i]);
	}

	iRCCE_wait_all(&general_waitlist);

	for (i=1; i < num_ranks; i++) {
		iRCCE_irecv((char*)&tries[i], sizeof(unsigned long long int), i, &recv_requests[i]);
		iRCCE_add_to_wait_list(&general_waitlist, NULL, &recv_requests[i]);
	}

	iRCCE_wait_all(&general_waitlist);

	free(recv_requests);
}

