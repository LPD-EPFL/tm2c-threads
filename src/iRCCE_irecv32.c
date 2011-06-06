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

static int iRCCE_push_recv32_request(iRCCE_RECV_REQUEST *request) {

    int test; // flag for calling iRCCE_test_flag()

    if (request->finished) return (iRCCE_SUCCESS);

    unsigned int my_offset = 32 * RCCE_NP;

    if (request->label == 2) goto label2;

    // receive data in units of available chunk size of MPB

    request->nbytes = 32;
    if (request->nbytes) {
label2:
        iRCCE_test_flag(*(request->sent), RCCE_FLAG_SET, &test);
        if (!test) {
            request->label = 2;
            return (iRCCE_PENDING);
        }
        request->started = 1;

        RCCE_flag_write(request->sent, RCCE_FLAG_UNSET, RCCE_IAM);
        // copy data from source's MPB space to private memory
        iRCCE_get((t_vcharp) request->privbuf, request->combuf + my_offset, request->nbytes, request->source);

        // tell the source I have moved data out of its comm buffer
        RCCE_flag_write(request->ready, RCCE_FLAG_SET, request->source);
    }


    request->finished = 1;
    return (iRCCE_SUCCESS);

}

static void iRCCE_init_recv32_request(
        char *privbuf, // source buffer in local private memory (send buffer)
        t_vcharp combuf, // intermediate buffer in MPB
        size_t chunk, // size of MPB available for this message (bytes)
        RCCE_FLAG *ready, // flag indicating whether receiver is ready
        RCCE_FLAG *sent, // flag indicating whether message has been sent by source
        size_t size, // size of message (bytes)
        int source, // UE that will send the message
        iRCCE_RECV_REQUEST *request
        ) {

    request->privbuf = privbuf;
    request->combuf = combuf;
    request->chunk = chunk;
    request->ready = ready;
    request->sent = sent;
    request->size = size;
    request->source = source;

    request->wsize = 0;
    request->remainder = 0;
    request->nbytes = 0;
    request->bufptr = NULL;

    request->label = 0;
    request->finished = 0;
    request->started = 0;

    request->next = NULL;

    return;
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_irecv
//--------------------------------------------------------------------------------------
// non-blocking recv function; returns an handle of type iRCCE_RECV_REQUEST
//--------------------------------------------------------------------------------------

int iRCCE_irecv32(char *privbuf, int source, iRCCE_RECV_REQUEST *request) {


    if (source < 0 || source >= RCCE_NP)
        return (RCCE_error_return(RCCE_debug_comm, RCCE_ERROR_ID));
    else {
        iRCCE_init_recv32_request(privbuf, RCCE_buff_ptr, RCCE_chunk,
                &RCCE_ready_flag[RCCE_IAM], &RCCE_sent_flag[source],
                32, source, request);

        if (iRCCE_irecv_queue[source] == NULL) {

            if (iRCCE_push_recv32_request(request) == iRCCE_SUCCESS) {
                return (iRCCE_SUCCESS);
            }
            else {
                iRCCE_irecv_queue[source] = request;

                return (iRCCE_PENDING);
            }
        }
        else {
            if (iRCCE_irecv_queue[source]->next == NULL) {
                iRCCE_irecv_queue[source]->next = request;
            }
            else {
                iRCCE_RECV_REQUEST *run = iRCCE_irecv_queue[source];
                while (run->next != NULL) run = run->next;
                run->next = request;
            }

            return (iRCCE_RESERVED);
        }
    }
}

//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_irecv_test
//--------------------------------------------------------------------------------------
// test function for completion of the requestes non-blocking recv operation
// Just provide NULL instead of the testvar if you don't need it
//--------------------------------------------------------------------------------------

int iRCCE_irecv32_test(iRCCE_RECV_REQUEST *request, int *test) {

    int source;

    if (request == NULL) {

        if (iRCCE_irecv32_push() == iRCCE_SUCCESS) {
            if (test) (*test) = 1;
            return (iRCCE_SUCCESS);
        }
        else {
            if (test) (*test) = 0;
            return (iRCCE_PENDING);
        }
    }

    source = request->source;

    if (request->finished) {
        if (test) (*test) = 1;
        return (iRCCE_SUCCESS);
    }

    if (iRCCE_irecv_queue[source] != request) {
        if (test) (*test) = 0;
        return (iRCCE_RESERVED);
    }

    iRCCE_push_recv32_request(request);

    if (request->finished) {
        iRCCE_irecv_queue[source] = request->next;

        if (test) (*test) = 1;
        return (iRCCE_SUCCESS);
    }

    if (test) (*test) = 0;
    return (iRCCE_PENDING);
}


//--------------------------------------------------------------------------------------
// FUNCTION: iRCCE_irecv_push
//--------------------------------------------------------------------------------------
// progress function for pending requests in the irecv queue
//--------------------------------------------------------------------------------------

static int iRCCE_irecv32_push_source(int source) {

    iRCCE_RECV_REQUEST *request = iRCCE_irecv_queue[source];

    if (request == NULL) {
        return (iRCCE_SUCCESS);
    }

    if (request->finished) {
        return (iRCCE_SUCCESS);
    }

    iRCCE_push_recv32_request(request);

    if (request->finished) {
        iRCCE_irecv_queue[source] = request->next;
        return (iRCCE_SUCCESS);
    }

    return (iRCCE_PENDING);
}

int iRCCE_irecv32_push(void) {

    int i, j;
    int retval = iRCCE_SUCCESS;

    for (i = 0; i < RCCE_NP; i++) {

        j = iRCCE_irecv32_push_source(i);

        if (j != iRCCE_SUCCESS) {
            retval = j;
        }
    }

    return retval;
}
