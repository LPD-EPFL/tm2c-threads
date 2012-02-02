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

static int iRCCE_push_send32_request(iRCCE_SEND_REQUEST *request) {

    int test; // flag for calling iRCCE_test_flag()

    if (request->finished) return (iRCCE_SUCCESS);

    unsigned int receiver_offset = 32 * request->dest;

    if (request->label == 2) goto label2;

    // send remainder of data--whole cache lines
    request->nbytes = 32;
    if (request->nbytes) {
        // copy private data to own comm buffer
        iRCCE_put(request->combuf + receiver_offset, (t_vcharp) request->privbuf, request->nbytes, RCCE_IAM);
        RCCE_flag_write(request->sent, RCCE_FLAG_SET, request->dest);
        // wait for the destination to be ready to receive a message
label2:
        iRCCE_test_flag(*(request->ready), RCCE_FLAG_SET, &test);
        if (!test) {
            request->label = 2;
            return (iRCCE_PENDING);
        }
        RCCE_flag_write(request->ready, RCCE_FLAG_UNSET, RCCE_IAM);
    }

    request->finished = 1;
    return (iRCCE_SUCCESS);
}

static void iRCCE_init_send32_request(
        char *privbuf, // source buffer in local private memory (send buffer)
        t_vcharp combuf, // intermediate buffer in MPB
        size_t chunk, // size of MPB available for this message (bytes)
        RCCE_FLAG *ready, // flag indicating whether receiver is ready
        RCCE_FLAG *sent, // flag indicating whether message has been sent by source
        size_t size, // size of message (bytes)
        int dest, // UE that will receive the message
        iRCCE_SEND_REQUEST *request
        ) {

    request->privbuf = privbuf;
    request->combuf = combuf;
    request->chunk = chunk;
    request->ready = ready;
    request->sent = sent;
    request->size = size;
    request->dest = dest;

    request->wsize = 0;
    request->remainder = 0;
    request->nbytes = 0;
    request->bufptr = NULL;

    request->label = 0;

    request->finished = 0;

    request->next = NULL;

    return;
}

//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_isend
//--------------------------------------------------------------------------------------
// non-blocking send function; returns a handle of type iRCCE_SEND_REQUEST
//--------------------------------------------------------------------------------------

int iRCCE_isend32(char *privbuf, int dest, iRCCE_SEND_REQUEST *request) {

    if (dest < 0 || dest >= RCCE_NP)
        return (RCCE_error_return(RCCE_debug_comm, RCCE_ERROR_ID));
    else {
        iRCCE_init_send32_request(privbuf, RCCE_buff_ptr, RCCE_chunk,
                &RCCE_ready_flag[dest], &RCCE_sent_flag[RCCE_IAM],
                32, dest, request);

        if (iRCCE_isend_queue32[dest] == NULL) {

            if (iRCCE_push_send32_request(request) == iRCCE_SUCCESS) {
                return (iRCCE_SUCCESS);
            }
            else {
                iRCCE_isend_queue32[dest] = request;

                return (iRCCE_PENDING);
            }
        }
        else {
            if (iRCCE_isend_queue32[dest]->next == NULL) {
                iRCCE_isend_queue32[dest]->next = request;
            }
            else {
                iRCCE_SEND_REQUEST *run = iRCCE_isend_queue32[dest];
                while (run->next != NULL) run = run->next;
                run->next = request;
            }

            return (iRCCE_RESERVED);
        }
    }
}

//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_isend_test
//--------------------------------------------------------------------------------------
// test function for completion of the requestes non-blocking send operation
// Just provide NULL instead of testvar if you don't need it
//--------------------------------------------------------------------------------------

int iRCCE_isend32_test(iRCCE_SEND_REQUEST *request, int *test) {

    if (request == NULL) {

        iRCCE_isend32_push();

        if (iRCCE_isend_queue32[request->dest] == NULL) {
            if (test) (*test) = 1;
            return (iRCCE_SUCCESS);
        }
        else {
            if (test) (*test) = 0;
            return (iRCCE_PENDING);
        }
    }

    if (request->finished) {
        if (test) (*test) = 1;
        return (iRCCE_SUCCESS);
    }

    if (iRCCE_isend_queue32[request->dest] != request) {
        if (test) (*test) = 0;
        return (iRCCE_RESERVED);
    }

    iRCCE_push_send32_request(request);

    if (request->finished) {
        iRCCE_isend_queue32[request->dest] = request->next;

        if (test) (*test) = 1;
        return (iRCCE_SUCCESS);
    }

    if (test) (*test) = 0;
    return (iRCCE_PENDING);
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_isend_push
//--------------------------------------------------------------------------------------
// progress function for pending requests in the isend queue
//--------------------------------------------------------------------------------------

int iRCCE_isend32_push(void) {
    
    int i, retval = iRCCE_SUCCESS, k;

    for (i = 0; i < RCCE_NP; i++) {
        iRCCE_SEND_REQUEST * request = iRCCE_isend_queue32[i];
        if (request == NULL) {
            continue;
        }

        if (request->finished) {
            continue;
        }

        k = iRCCE_push_send32_request(request);
        if (k != iRCCE_SUCCESS) {
            retval = k;
        }

        if (request->finished) {
            iRCCE_isend_queue32[i] = request->next;
            continue;
        }
    }

    return retval;
}

