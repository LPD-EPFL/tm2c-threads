//***************************************************************************************
// Non-blocking send routines. 
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

#include "iRCCE_lib.h"

static int iRCCE_push_send_request(iRCCE_SEND_REQUEST *request) {

	char padline[RCCE_LINE_SIZE]; // copy buffer, used if message not multiple of line size
	int   test;       // flag for calling iRCCE_test_flag()

	if(request->finished) return(iRCCE_SUCCESS);

	if(request->label == 1) goto label1;
	if(request->label == 2) goto label2;
	if(request->label == 3) goto label3;

	// send data in units of available chunk size of comm buffer 
	for (; request->wsize< (request->size / request->chunk) * request->chunk; request->wsize += request->chunk) {
		request->bufptr = request->privbuf + request->wsize;
		request->nbytes = request->chunk;
		// copy private data to own comm buffer
		iRCCE_put(request->combuf, (t_vcharp) request->bufptr, request->nbytes, RCCE_IAM);
		RCCE_flag_write(request->sent, RCCE_FLAG_SET, request->dest);
		// wait for the destination to be ready to receive a message          
label1:
		iRCCE_test_flag(*(request->ready), RCCE_FLAG_SET, &test);
		if(!test) {
			request->label = 1;
			return(iRCCE_PENDING);
		}
		RCCE_flag_write(request->ready, RCCE_FLAG_UNSET, RCCE_IAM);
	}

	//if (request->size < request->chunk) {
          //  request->remainder = 0;
        //}
        //else {
            request->remainder = request->size % request->chunk;
        //}
	// if nothing is left over, we are done 
	if (!request->remainder) {
		request->finished = 1;
		return(iRCCE_SUCCESS);
	}

	// send remainder of data--whole cache lines            
	request->bufptr = request->privbuf + (request->size / request->chunk) * request->chunk;
	request->nbytes = request->remainder - request->remainder % RCCE_LINE_SIZE;
	if (request->nbytes) {
		// copy private data to own comm buffer
		iRCCE_put(request->combuf, (t_vcharp)request->bufptr, request->nbytes, RCCE_IAM);
		RCCE_flag_write(request->sent, RCCE_FLAG_SET, request->dest);
		// wait for the destination to be ready to receive a message          
label2:
		iRCCE_test_flag(*(request->ready), RCCE_FLAG_SET, &test);
		if(!test) {
			request->label = 2;
			return(iRCCE_PENDING);
		}
		RCCE_flag_write(request->ready, RCCE_FLAG_UNSET, RCCE_IAM);
	}

	request->remainder = request->size % request->chunk; 
	request->remainder = request->remainder%RCCE_LINE_SIZE;
	// if nothing is left over, we are done 
	if (!request->remainder)
	{
		request->finished = 1;
		return(iRCCE_SUCCESS);
	}

	// remainder is less than a cache line. This must be copied into appropriately sized 
	// intermediate space before it can be sent to the receiver 
	request->bufptr = request->privbuf + (request->size / request->chunk) * request->chunk + request->nbytes;
	request->nbytes = RCCE_LINE_SIZE;
	// copy private data to own comm buffer 
	memcpy(padline,request->bufptr,request->remainder);
	iRCCE_put(request->combuf, (t_vcharp)padline, request->nbytes, RCCE_IAM);
	RCCE_flag_write(request->sent, RCCE_FLAG_SET, request->dest);
	// wait for the destination to be ready to receive a message          
label3:
	iRCCE_test_flag(*(request->ready), RCCE_FLAG_SET, &test);
	if(!test) {
		request->label = 3;
		return(iRCCE_PENDING);
	}
	RCCE_flag_write(request->ready, RCCE_FLAG_UNSET, RCCE_IAM);

	request->finished = 1;
	return(iRCCE_SUCCESS);
}

static void iRCCE_init_send_request(
		char *privbuf,    // source buffer in local private memory (send buffer)
		t_vcharp combuf,  // intermediate buffer in MPB
		size_t chunk,     // size of MPB available for this message (bytes)
		RCCE_FLAG *ready, // flag indicating whether receiver is ready
		RCCE_FLAG *sent,  // flag indicating whether message has been sent by source
		size_t size,      // size of message (bytes)
		int dest,         // UE that will receive the message
		iRCCE_SEND_REQUEST *request
		) {

	request->privbuf  = privbuf;
	request->combuf   = combuf;
	request->chunk    = chunk;
	request->ready    = ready;
	request->sent     = sent;
	request->size     = size;
	request->dest     = dest;

	request->wsize     = 0;
	request->remainder = 0;
	request->nbytes    = 0;
	request->bufptr    = NULL;

	request->label    = 0;

	request->finished = 0;

	request->next     = NULL;

	return;
}

//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_isend
//--------------------------------------------------------------------------------------
// non-blocking send function; returns a handle of type iRCCE_SEND_REQUEST
//--------------------------------------------------------------------------------------
static iRCCE_SEND_REQUEST blocking_isend_request;
int iRCCE_isend(char *privbuf, size_t size, int dest, iRCCE_SEND_REQUEST *request) {

	if(request == NULL) request = &blocking_isend_request;

	if (dest<0 || dest >= RCCE_NP) 
		return(RCCE_error_return(RCCE_debug_comm,RCCE_ERROR_ID));
	else {
		iRCCE_init_send_request(privbuf, RCCE_buff_ptr, RCCE_chunk, 
				&RCCE_ready_flag[dest], &RCCE_sent_flag[RCCE_IAM], 
				size, dest, request);

		if(iRCCE_isend_queue == NULL) {

			if(iRCCE_push_send_request(request) == iRCCE_SUCCESS) {
				return(iRCCE_SUCCESS);
			}
			else {
				iRCCE_isend_queue = request;

				if(request == &blocking_isend_request) {
					iRCCE_isend_wait(request);
					return(iRCCE_SUCCESS);
				}

				return(iRCCE_PENDING);
			}
		}
		else {
			if(iRCCE_isend_queue->next == NULL) {
				iRCCE_isend_queue->next = request;
			}
			else {
				iRCCE_SEND_REQUEST *run = iRCCE_isend_queue;
				while(run->next != NULL) run = run->next;      
				run->next = request;   
			}

			if(request == &blocking_isend_request) {
				iRCCE_isend_wait(request);
				return(iRCCE_SUCCESS);
			}

			return(iRCCE_RESERVED);
		}
	}
}

//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_isend_test
//--------------------------------------------------------------------------------------
// test function for completion of the requestes non-blocking send operation
// Just provide NULL instead of testvar if you don't need it
//--------------------------------------------------------------------------------------
int iRCCE_isend_test(iRCCE_SEND_REQUEST *request, int *test) {

	if(request == NULL) {

		iRCCE_isend_push();

		if(iRCCE_isend_queue == NULL) {
			if (test) (*test) = 1;
			return(iRCCE_SUCCESS);
		}
		else {
			if (test) (*test) = 0;
			return(iRCCE_PENDING);
		}    
	}

	if(request->finished) {
		if (test) (*test) = 1;
		return(iRCCE_SUCCESS);
	}

	if(iRCCE_isend_queue != request) {
		if (test) (*test) = 0;
		return(iRCCE_RESERVED);
	}

	iRCCE_push_send_request(request);   

	if(request->finished) {
		iRCCE_isend_queue = request->next;

	 if (test) (*test) = 1;
		return(iRCCE_SUCCESS);
	}

	if (test) (*test) = 0;
	return(iRCCE_PENDING);
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_isend_push
//--------------------------------------------------------------------------------------
// progress function for pending requests in the isend queue 
//--------------------------------------------------------------------------------------
int iRCCE_isend_push(void) {

	iRCCE_SEND_REQUEST *request = iRCCE_isend_queue;

	if(request == NULL) {
		return(iRCCE_SUCCESS);
	}

	if(request->finished) {
		return(iRCCE_SUCCESS);
	}

	iRCCE_push_send_request(request);   

	if(request->finished) {
		iRCCE_isend_queue = request->next;   
		return(iRCCE_SUCCESS);
	}

	return(iRCCE_PENDING);
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_isend_wait
//--------------------------------------------------------------------------------------
// just wait for completion of the requestes non-blocking send operation
//--------------------------------------------------------------------------------------
int iRCCE_isend_wait(iRCCE_SEND_REQUEST *request) {

	if(request != NULL) {

		while(!request->finished) {

			iRCCE_isend_push();
			iRCCE_irecv_push();      
		}
	}
	else {

		while(iRCCE_isend_queue != NULL) {

			iRCCE_isend_push();     
			iRCCE_irecv_push();     
		}
	}

	return(iRCCE_SUCCESS);
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_isend_cancel
//--------------------------------------------------------------------------------------
// try to cancel a pending non-blocking send request
//--------------------------------------------------------------------------------------
int iRCCE_isend_cancel(iRCCE_SEND_REQUEST *request, int *test) {
  
	iRCCE_SEND_REQUEST *run;
  
	if( (request == NULL) || (request->finished) ) {
		if (test) (*test) = 0;
		return iRCCE_NOT_ENQUEUED;
	}
  
	if(iRCCE_isend_queue == NULL) {
		if (test) (*test) = 0;
		return iRCCE_NOT_ENQUEUED;
	}
  
	if(iRCCE_isend_queue == request) {
		if (test) (*test) = 0;
		return iRCCE_PENDING;
	}
 
	for(run = iRCCE_isend_queue; run->next != NULL; run = run->next) {
    
		// request found --> remove it from send queue:
		if(run->next == request) {
      
			run->next = run->next->next;
      
			if (test) (*test) = 1;
			return iRCCE_SUCCESS;
		}
	}
  
	if (test) (*test) = 0;
	return iRCCE_NOT_ENQUEUED;
}
