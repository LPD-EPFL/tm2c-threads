
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

#include "iRCCE.h"

#define NUMROUNDS   3

int * send_numbers;
int * recv_numbers;
char dummy = 0;
iRCCE_WAIT_LIST general_waitlist;

//
//       THIS CODE DOES _NOT_ WORK WITH THE OPENMP RCCE EMULATOR!
//
#ifdef _OPENMP
#warning THIS CODE DOES _NOT_ WORK WITH THE OPENMP RCCE EMULATOR!
#endif
//
//

int RCCE_APP(int argc, char **argv)
{
	int i;
	int num_ranks;
	int remote_rank, my_rank;
	int numrounds = NUMROUNDS;
	int length = 4;
	int round;
	int sum;
	double timer;
	iRCCE_SEND_REQUEST *send_requests;
	iRCCE_RECV_REQUEST *recv_requests;

	RCCE_init(&argc, &argv);
	iRCCE_init();

	my_rank   = RCCE_ue();
	num_ranks = RCCE_num_ues();

	if(argc > 1) numrounds = atoi(argv[1]);

	if(numrounds < 1)
	{
		if(my_rank == 0) fprintf(stderr, "SPAM needs at least 1 round; try again\n");
		exit(-1);
	}

	if(num_ranks != 2)
	{
		if(my_rank == 0) fprintf(stderr, "SPAM needs exactly two UEs; try again\n");
		exit(-1);
	}

	iRCCE_init_wait_list(&general_waitlist);
	send_requests = (iRCCE_SEND_REQUEST*)malloc(numrounds*sizeof(iRCCE_SEND_REQUEST));
	recv_requests = (iRCCE_RECV_REQUEST*)malloc(numrounds*sizeof(iRCCE_RECV_REQUEST));
	send_numbers = (int*)malloc(numrounds*sizeof(int));
	recv_numbers = (int*)malloc(numrounds*sizeof(int));

	if (my_rank == 0) printf("Will send numbers from 1 to %d.\n", numrounds);

	remote_rank = (my_rank + 1) % 2;

	/* synchronize before starting SPAM: */
	RCCE_barrier(&RCCE_COMM_WORLD);

	sum = 0;
	for(round=0; round < numrounds; round++)
	{
		if (my_rank == 0) {
			send_numbers[round] = round+1;
			iRCCE_isend((char*)&send_numbers[round], length, remote_rank, &send_requests[round]);
			iRCCE_add_to_wait_list(&general_waitlist, &send_requests[round], NULL);
		}
		else {
			iRCCE_irecv((char*)&recv_numbers[round], length, remote_rank, &recv_requests[round]);
			iRCCE_add_to_wait_list(&general_waitlist, NULL, &recv_requests[round]);
		}
	}

	iRCCE_wait_all(&general_waitlist);

	if (my_rank == 0 ) {
		printf("Sending done.\n");
	}
	else {
	  for(round=numrounds-1; round >= 0; round--) sum += recv_numbers[round];			

		if (sum == (numrounds * (numrounds+1) /2))
			printf("Received right sum: %d.\n", sum);
		else
			printf("Received wrong sum: %d - should be %d.\n", sum, numrounds * (numrounds+1) / 2);
	}

	RCCE_finalize();

	return 0;
}

