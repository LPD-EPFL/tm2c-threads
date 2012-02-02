//***************************************************************************************
// Non-blocking receive routines. 
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
//    [2010-11-26] added a _pipelined_ version of blocking send/recv
//                 by Carsten Clauss, Chair for Operating Systems,
//                                    RWTH Aachen University
//
#include "iRCCE_lib.h"
#include <stdlib.h>
#include <string.h>

//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_recv_general
//--------------------------------------------------------------------------------------
// pipelined receive function
//--------------------------------------------------------------------------------------
static int iRCCE_recv_general(
		char *privbuf,    // destination buffer in local private memory (receive buffer)
		t_vcharp combuf,  // intermediate buffer in MPB
		size_t chunk,     // size of MPB available for this message (bytes)
		RCCE_FLAG *ready, // flag indicating whether receiver is ready
		RCCE_FLAG *sent,  // flag indicating whether message has been sent by source
		size_t size,      // size of message (bytes)
		int source,       // UE that sent the message
		int *test        // if 1 upon entry, do nonblocking receive; if message available
		// set to 1, otherwise to 0
		) {

	char padline[RCCE_LINE_SIZE]; // copy buffer, used if message not multiple of line size
	size_t wsize,   // offset within receive buffer when pulling in "chunk" bytes
				 remainder, // bytes remaining to be received
				 nbytes;    // number of bytes to be received in single iRCCE_get call
	int first_test; // only use first chunk to determine if message has been received yet
	char *bufptr;   // running pointer inside privbuf for current location

	first_test = 1;

#if 0
	// receive data in units of available chunk size of MPB 
	for (wsize=0; wsize< (size/chunk)*chunk; wsize+=chunk) {
		bufptr = privbuf + wsize;
		nbytes = chunk;
		// if function is called in test mode, check if first chunk has been sent already. 
		// If so, proceed as usual. If not, exit immediately 
		if (*test && first_test) {
			first_test = 0;
			if (!(*test = RCCE_probe(*sent))) return(iRCCE_SUCCESS);
		}
		RCCE_wait_until(*sent, RCCE_FLAG_SET);
		RCCE_flag_write(sent, RCCE_FLAG_UNSET, RCCE_IAM);
		// copy data from local MPB space to private memory 
		iRCCE_get((t_vcharp)bufptr, combuf, nbytes, source);

		// tell the source I have moved data out of its comm buffer
		RCCE_flag_write(ready, RCCE_FLAG_SET, source);
	}
#else
	{ // pipelined version of send/recv:

		size_t subchunk1 = chunk / 2;
		size_t subchunk2 = chunk - subchunk1;

		for (wsize=0; wsize < (size/chunk)*chunk; wsize+=chunk) {

			if (*test && first_test) {
				first_test = 0;
				if (!(*test = RCCE_probe(*sent))) return(iRCCE_SUCCESS);
			}    

			bufptr = privbuf + wsize;
			nbytes = subchunk1;

			RCCE_wait_until(*ready, RCCE_FLAG_SET);
			RCCE_flag_write(ready, RCCE_FLAG_UNSET, RCCE_IAM);
			iRCCE_get((t_vcharp)bufptr, combuf, nbytes, source);

			RCCE_flag_write(ready, RCCE_FLAG_SET, source);      

			bufptr = privbuf + wsize + subchunk1;
			nbytes = subchunk2;

			RCCE_wait_until(*sent, RCCE_FLAG_SET);
			RCCE_flag_write(sent, RCCE_FLAG_UNSET, RCCE_IAM);
			iRCCE_get((t_vcharp)bufptr, combuf + subchunk1, nbytes, source);

			RCCE_flag_write(sent, RCCE_FLAG_SET, source);     
		}   
	}
#endif

	remainder = size%chunk; 
	// if nothing is left over, we are done 
	if (!remainder) return(iRCCE_SUCCESS);

	// receive remainder of data--whole cache lines               
	bufptr = privbuf + (size/chunk)*chunk;
	nbytes = remainder - remainder % RCCE_LINE_SIZE;
	if (nbytes) {
		// if function is called in test mode, check if first chunk has been sent already. 
		// If so, proceed as usual. If not, exit immediately 
		if (*test && first_test) {
			first_test = 0;
			if (!(*test = RCCE_probe(*sent))) return(iRCCE_SUCCESS);
		}
		RCCE_wait_until(*sent, RCCE_FLAG_SET);
		RCCE_flag_write(sent, RCCE_FLAG_UNSET, RCCE_IAM);
		// copy data from local MPB space to private memory 
		iRCCE_get((t_vcharp)bufptr, combuf, nbytes, source);

		// tell the source I have moved data out of its comm buffer
		RCCE_flag_write(ready, RCCE_FLAG_SET, source);
	}

	remainder = remainder % RCCE_LINE_SIZE;
	if (!remainder) return(iRCCE_SUCCESS);

	// remainder is less than cache line. This must be copied into appropriately sized 
	// intermediate space before exact number of bytes get copied to the final destination 
	bufptr = privbuf + (size/chunk)*chunk + nbytes;
	nbytes = RCCE_LINE_SIZE;

	// if function is called in test mode, check if first chunk has been sent already. 
	// If so, proceed as usual. If not, exit immediately 
	if (*test && first_test) {
		first_test = 0;
		if (!(*test = RCCE_probe(*sent))) return(iRCCE_SUCCESS);
	}
	RCCE_wait_until(*sent, RCCE_FLAG_SET);
	RCCE_flag_write(sent, RCCE_FLAG_UNSET, RCCE_IAM);

	// copy data from local MPB space to private memory   
	iRCCE_get((t_vcharp)padline, combuf, nbytes, source);
	memcpy(bufptr,padline,remainder);    

	// tell the source I have moved data out of its comm buffer
	RCCE_flag_write(ready, RCCE_FLAG_SET, source);

	return(iRCCE_SUCCESS);
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_recv
//--------------------------------------------------------------------------------------
// pipelined recv function  (blocking!)
//--------------------------------------------------------------------------------------
int iRCCE_recv(char *privbuf, size_t size, int source) {
	int ignore;

	while(iRCCE_irecv_queue[source] != NULL) {
		// wait for completion of pending non-blocking requests
		iRCCE_irecv_push();
		iRCCE_isend_push();
	}

	if (source<0 || source >= RCCE_NP) 
		return(RCCE_error_return(RCCE_debug_comm,RCCE_ERROR_ID));
	else {
		ignore = 0;
		return(iRCCE_recv_general(privbuf, RCCE_buff_ptr, RCCE_chunk, 
					&RCCE_ready_flag[RCCE_IAM], &RCCE_sent_flag[source], 
					size, source, &ignore));
	}
}

