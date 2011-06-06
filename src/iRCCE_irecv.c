//***************************************************************************************
// Synchronized receive routines. 
//***************************************************************************************
//
// Author: Rob F. Van der Wijngaart
//         Intel Corporation
// Date:   008/30/2010
//
//***************************************************************************************
// 
// Copyright 2010 Intel Corporation
// 
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
// 
//        http://www.apache.org/licenses/LICENSE-2.0
// 
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
// 
//    [2010-10-25] added support for non-blocking send/recv operations
//                 - iRCCE_isend(), ..._test(), ..._wait(), ..._push()
//                 - iRCCE_irecv(), ..._test(), ..._wait(), ..._push()
//                 by Carsten Clauss, Chair for Operating Systems,
//                                    RWTH Aachen University
//
//    [2010-11-12] extracted non-blocking code into separate library
//                 by Carsten Scholtes
//
//    [2010-12-09] added cancel functions for non-blocking send/recv requests
//                 by Carsten Clauss
//
//    [2011-02-21] added support for multiple incoming queues
//                 (one recv queue per remote rank)
//                  

#include "iRCCE_lib.h"

static int iRCCE_push_recv_request(iRCCE_RECV_REQUEST *request) {

	char padline[RCCE_LINE_SIZE]; // copy buffer, used if message not multiple of line size
	int   test;                   // flag for calling iRCCE_test_flag()

	if(request->finished) return(iRCCE_SUCCESS);

	if(request->label == 1) goto label1;
	if(request->label == 2) goto label2;
	if(request->label == 3) goto label3;

	// receive data in units of available chunk size of MPB 
	for (; request->wsize < (request->size / request->chunk) * request->chunk; request->wsize += request->chunk) {
		request->bufptr = request->privbuf + request->wsize;
		request->nbytes = request->chunk;
label1:
		iRCCE_test_flag(*(request->sent), RCCE_FLAG_SET, &test);
		if(!test) {
			request->label = 1;
			return(iRCCE_PENDING);
		}
		request->started = 1;

		RCCE_flag_write(request->sent, RCCE_FLAG_UNSET, RCCE_IAM);
		// copy data from source's MPB space to private memory 
		iRCCE_get((t_vcharp)request->bufptr, request->combuf, request->nbytes, request->source);

		// tell the source I have moved data out of its comm buffer
		RCCE_flag_write(request->ready, RCCE_FLAG_SET, request->source);
	}

        //if (request->size < request->chunk) {
         //   request->remainder = 0;
       // }
        //else {
            request->remainder = request->size % request->chunk; 
        //}
	
	// if nothing is left over, we are done 
	if (!request->remainder) {
		request->finished = 1;
		return(iRCCE_SUCCESS);
	}

	// receive remainder of data--whole cache lines               
	request->bufptr = request->privbuf + (request->size / request->chunk) * request->chunk;
	request->nbytes = request->remainder - request->remainder % RCCE_LINE_SIZE;
	if (request->nbytes) {
label2:
		iRCCE_test_flag(*(request->sent), RCCE_FLAG_SET, &test);
		if(!test) {
			request->label = 2;
			return(iRCCE_PENDING);
		}
		request->started = 1;

		RCCE_flag_write(request->sent, RCCE_FLAG_UNSET, RCCE_IAM);
		// copy data from source's MPB space to private memory 
		iRCCE_get((t_vcharp)request->bufptr, request->combuf, request->nbytes, request->source);

		// tell the source I have moved data out of its comm buffer
		RCCE_flag_write(request->ready, RCCE_FLAG_SET, request->source);
	}

	request->remainder = request->size % request->chunk; 
	request->remainder = request->remainder % RCCE_LINE_SIZE;
	if (!request->remainder) {
		request->finished = 1;
		return(iRCCE_SUCCESS);
	}

	// remainder is less than cache line. This must be copied into appropriately sized 
	// intermediate space before exact number of bytes get copied to the final destination 
	request->bufptr = request->privbuf + (request->size / request->chunk) * request->chunk + request->nbytes;
	request->nbytes = RCCE_LINE_SIZE;
label3:
	iRCCE_test_flag(*(request->sent), RCCE_FLAG_SET, &test);
	if(!test) {
		request->label = 3;
		return(iRCCE_PENDING);
	}
	request->started = 1;

	RCCE_flag_write(request->sent, RCCE_FLAG_UNSET, RCCE_IAM);
	// copy data from source's MPB space to private memory   
	iRCCE_get((t_vcharp)padline, request->combuf, request->nbytes, request->source);
	memcpy(request->bufptr,padline,request->remainder);

	// tell the source I have moved data out of its comm buffer
	RCCE_flag_write(request->ready, RCCE_FLAG_SET, request->source);

	request->finished = 1;
	return(iRCCE_SUCCESS);
}

static void iRCCE_init_recv_request(
		char *privbuf,    // source buffer in local private memory (send buffer)
		t_vcharp combuf,  // intermediate buffer in MPB
		size_t chunk,     // size of MPB available for this message (bytes)
		RCCE_FLAG *ready, // flag indicating whether receiver is ready
		RCCE_FLAG *sent,  // flag indicating whether message has been sent by source
		size_t size,      // size of message (bytes)
		int source,       // UE that will send the message
		iRCCE_RECV_REQUEST *request
		) {

	request->privbuf   = privbuf;
	request->combuf    = combuf;
	request->chunk     = chunk;
	request->ready     = ready;
	request->sent      = sent;
	request->size      = size;
	request->source    = source;

	request->wsize     = 0;
	request->remainder = 0;
	request->nbytes    = 0;
	request->bufptr    = NULL;

	request->label     = 0;
	request->finished  = 0;
	request->started   = 0;

	request->next      = NULL;

	return;
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_irecv
//--------------------------------------------------------------------------------------
// non-blocking recv function; returns an handle of type iRCCE_RECV_REQUEST
//--------------------------------------------------------------------------------------
static iRCCE_RECV_REQUEST blocking_irecv_request;
int iRCCE_irecv(char *privbuf, size_t size, int source, iRCCE_RECV_REQUEST *request) {

	if(request == NULL) request = &blocking_irecv_request;

	if (source<0 || source >= RCCE_NP) 
		return(RCCE_error_return(RCCE_debug_comm,RCCE_ERROR_ID));
	else {
		iRCCE_init_recv_request(privbuf, RCCE_buff_ptr, RCCE_chunk, 
				&RCCE_ready_flag[RCCE_IAM], &RCCE_sent_flag[source], 
				size, source, request);

		if(iRCCE_irecv_queue[source] == NULL) {

			if(iRCCE_push_recv_request(request) == iRCCE_SUCCESS) {
				return(iRCCE_SUCCESS);
			}
			else {       
				iRCCE_irecv_queue[source] = request;

				if(request == &blocking_irecv_request) {
					iRCCE_irecv_wait(request);
					return(iRCCE_SUCCESS);
				}

				return(iRCCE_PENDING);
			}
		}
		else {
			if(iRCCE_irecv_queue[source]->next == NULL) {
				iRCCE_irecv_queue[source]->next = request;
			}
			else {
				iRCCE_RECV_REQUEST *run = iRCCE_irecv_queue[source];
				while(run->next != NULL) run = run->next;      
				run->next = request;   
			}

				if(request == &blocking_irecv_request) {
					iRCCE_irecv_wait(request);
					return(iRCCE_SUCCESS);
				}

			return(iRCCE_RESERVED);
		}
	}
}

//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_irecv_test
//--------------------------------------------------------------------------------------
// test function for completion of the requestes non-blocking recv operation
// Just provide NULL instead of the testvar if you don't need it
//--------------------------------------------------------------------------------------
int iRCCE_irecv_test(iRCCE_RECV_REQUEST *request, int *test) {

	int source;

	if(request == NULL) {

		if(iRCCE_irecv_push() == iRCCE_SUCCESS) {
			if (test) (*test) = 1;
			return(iRCCE_SUCCESS);
		}
		else {
			if (test) (*test) = 0;
			return(iRCCE_PENDING);
		}    
	}

	source = request->source;

	if(request->finished) {
		if (test) (*test) = 1;
		return(iRCCE_SUCCESS);
	}

	if(iRCCE_irecv_queue[source] != request) {
		if (test) (*test) = 0;
		return(iRCCE_RESERVED);
	}

	iRCCE_push_recv_request(request);

	if(request->finished) {
		iRCCE_irecv_queue[source] = request->next;

		if (test) (*test) = 1;
		return(iRCCE_SUCCESS);
	}

	if (test) (*test) = 0;
	return(iRCCE_PENDING);
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_irecv_push
//--------------------------------------------------------------------------------------
// progress function for pending requests in the irecv queue 
//--------------------------------------------------------------------------------------
static int iRCCE_irecv_push_source(int source) {

	iRCCE_RECV_REQUEST *request = iRCCE_irecv_queue[source];

	if(request == NULL) {
		return(iRCCE_SUCCESS);
	}

	if(request->finished) {
		return(iRCCE_SUCCESS);
	}

	iRCCE_push_recv_request(request);   

	if(request->finished) {    
		iRCCE_irecv_queue[source] = request->next;
		return(iRCCE_SUCCESS);
	}

	return(iRCCE_PENDING);
}

int iRCCE_irecv_push(void) {

	int i, j; 
	int retval = iRCCE_SUCCESS;

	for(i=0; i<RCCE_NP; i++) {

		j = iRCCE_irecv_push_source(i);

		if(j != iRCCE_SUCCESS) {
			retval = j;
		}
	}

	return retval;
}

//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_irecv_wait
//--------------------------------------------------------------------------------------
// just wait for completion of the requested non-blocking send operation
//--------------------------------------------------------------------------------------
int iRCCE_irecv_wait(iRCCE_RECV_REQUEST *request) {

	if(request != NULL) {
		while(!request->finished) {
			iRCCE_irecv_push();
			iRCCE_isend_push();
		}
	}
	else {
		do {
			iRCCE_isend_push();
		}
		while(  iRCCE_irecv_push() != iRCCE_SUCCESS );
	}

	return(iRCCE_SUCCESS);
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_irecv_cancel
//--------------------------------------------------------------------------------------
// try to cancel a pending non-blocking recv request
//--------------------------------------------------------------------------------------
int iRCCE_irecv_cancel(iRCCE_RECV_REQUEST *request, int *test) {
  
	int source;
	iRCCE_RECV_REQUEST *run;
  
	if( (request == NULL) || (request->finished) ) {
		if (test) (*test) = 0;
		return iRCCE_NOT_ENQUEUED;
	}

	source = request->source;
  
	if(iRCCE_irecv_queue[source] == NULL) {
		if (test) (*test) = 0;
		return iRCCE_NOT_ENQUEUED;
	}
  
	if(iRCCE_irecv_queue[source] == request) {

		// have parts of the message already been received?
		if(request->started) {
			if (test) (*test) = 0;
			return iRCCE_PENDING;
		}
		else {
			// no, thus request can be canceld just in time:
			iRCCE_irecv_queue[source] = request->next;
			if (test) (*test) = 1;
			return iRCCE_SUCCESS;
		}
	}
 
	for(run = iRCCE_irecv_queue[source]; run->next != NULL; run = run->next) {
    
		// request found --> remove it from recv queue:
		if(run->next == request) {
      
			run->next = run->next->next;
      
			if (test) (*test) = 1;
			return iRCCE_SUCCESS;
		}
	}
  
	if (test) (*test) = 0;
	return iRCCE_NOT_ENQUEUED;
}
