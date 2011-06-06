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
//                 by Carsten Scholtes, University of Bayreuth
//
//    [2010-12-09] added functions for a convenient handling of multiple
//                 pending non-blocking requests
//                 by Jacek Galowicz, Chair for Operating Systems
//                                    RWTH Aachen University
//
#ifndef IRCCE_H
#define IRCCE_H

#include "RCCE.h"

#define iRCCE_SUCCESS  RCCE_SUCCESS
#define iRCCE_PENDING      -1
#define iRCCE_RESERVED     -2
#define iRCCE_NOT_ENQUEUED -3

typedef struct _iRCCE_SEND_REQUEST {
  char *privbuf;    // source buffer in local private memory (send buffer)
  t_vcharp combuf;  // intermediate buffer in MPB
  size_t chunk;     // size of MPB available for this message (bytes)
  RCCE_FLAG *ready; // flag indicating whether receiver is ready
  RCCE_FLAG *sent;  // flag indicating whether message has been sent by source
  size_t size;      // size of message (bytes)
  int dest;         // UE that will receive the message

  size_t wsize;     // offset within send buffer when putting in "chunk" bytes
  size_t remainder;  // bytes remaining to be sent
  size_t nbytes;    // number of bytes to be sent in single RCCE_put call
  char *bufptr;     // running pointer inside privbuf for current location

  int label;        // jump/goto label for the reentrance of the respective poll function
  int finished;     // flag that indicates whether the request has already been finished

  struct _iRCCE_SEND_REQUEST *next;
} iRCCE_SEND_REQUEST;


typedef struct _iRCCE_RECV_REQUEST {
  char *privbuf;    // source buffer in local private memory (send buffer)
  t_vcharp combuf;  // intermediate buffer in MPB
  size_t chunk;     // size of MPB available for this message (bytes)
  RCCE_FLAG *ready; // flag indicating whether receiver is ready
  RCCE_FLAG *sent;  // flag indicating whether message has been sent by source
  size_t size;      // size of message (bytes)
  int source;       // UE that will send the message

  size_t wsize;     // offset within send buffer when putting in "chunk" bytes
  size_t remainder; // bytes remaining to be sent
  size_t nbytes;    // number of bytes to be sent in single RCCE_put call
  char *bufptr;     // running pointer inside privbuf for current location

  int label;        // jump/goto label for the reentrance of the respective poll function
  int finished;     // flag that indicates whether the request has already been finished
  int started;      // flag that indicates whether message parts have already been received

  struct _iRCCE_RECV_REQUEST *next;
} iRCCE_RECV_REQUEST;

#define iRCCE_WAIT_LIST_RECV_TYPE 0
#define iRCCE_WAIT_LIST_SEND_TYPE 1

typedef struct _iRCCE_WAIT_LISTELEM {
	int type;
	struct _iRCCE_WAIT_LISTELEM * next;
	void * req;
} iRCCE_WAIT_LISTELEM;

typedef struct _iRCCE_WAIT_LIST {
	iRCCE_WAIT_LISTELEM * first;
	iRCCE_WAIT_LISTELEM * last;
} iRCCE_WAIT_LIST;


///////////////////////////////////////////////////////////////
//
//                       THE iRCCE API:
//
//  Initialize function:
int   iRCCE_init(void);
//
//  Non-blocking send/recv functions:
int   iRCCE_isend(char *, size_t, int, iRCCE_SEND_REQUEST *);
int   iRCCE_isend_test(iRCCE_SEND_REQUEST *, int *);
int   iRCCE_isend_wait(iRCCE_SEND_REQUEST *);
int   iRCCE_isend_push(void);
int   iRCCE_irecv(char *, size_t, int, iRCCE_RECV_REQUEST *);
int   iRCCE_irecv_test(iRCCE_RECV_REQUEST *, int *);
int   iRCCE_irecv_wait(iRCCE_RECV_REQUEST *);
int   iRCCE_irecv_push(void);
//
//  Non-blocking send/recv functions: CUSTOMIZED 32 byte version
int   iRCCE_isend32(char *, int, iRCCE_SEND_REQUEST *);
int   iRCCE_isend32_test(iRCCE_SEND_REQUEST *, int *);
int   iRCCE_isend32_push(void);
int   iRCCE_irecv32(char *, int, iRCCE_RECV_REQUEST *);
int   iRCCE_irecv32_test(iRCCE_RECV_REQUEST *, int *);
int   iRCCE_irecv32_push(void);
//
//  Blocking but pipelined send/recv functions:
int   iRCCE_send(char *, size_t, int);
int   iRCCE_recv(char *, size_t, int);
//
//  SCC-customized put/get and memcpy functions:
int   iRCCE_put(t_vcharp, t_vcharp, int, int);
int   iRCCE_get(t_vcharp, t_vcharp, int, int);
void* iRCCE_memcpy_put(void*, const void*, size_t);
void* iRCCE_memcpy_get(void*, const void*, size_t);
//
//  Wait/test-all/any functions:
unsigned int iRCCE_wait_list_length(iRCCE_WAIT_LIST *);
void iRCCE_print_sendlist(iRCCE_WAIT_LIST *list, unsigned int sender);
void iRCCE_print_waitlist(iRCCE_WAIT_LIST *list, unsigned int sender);
void  iRCCE_init_wait_list(iRCCE_WAIT_LIST*);
void  iRCCE_add_to_wait_list(iRCCE_WAIT_LIST*, iRCCE_SEND_REQUEST *, iRCCE_RECV_REQUEST *);
int   iRCCE_test_all(iRCCE_WAIT_LIST*, int *);
int   iRCCE_wait_all(iRCCE_WAIT_LIST*);
int   iRCCE_test_any(iRCCE_WAIT_LIST*, iRCCE_SEND_REQUEST **, iRCCE_RECV_REQUEST **);
int   iRCCE_wait_any(iRCCE_WAIT_LIST*, iRCCE_SEND_REQUEST **, iRCCE_RECV_REQUEST **);
//
//  Cancel functions for yet not started non-blocking requests:
int   iRCCE_isend_cancel(iRCCE_SEND_REQUEST *, int *);
int   iRCCE_irecv_cancel(iRCCE_RECV_REQUEST *, int *);
//
///////////////////////////////////////////////////////////////
//
//      Just for for convenience:
#if 1
#define RCCE_isend        iRCCE_isend
#define RCCE_isend_test   iRCCE_isend_test
#define RCCE_isend_wait   iRCCE_isend_wait
#define RCCE_isend_push   iRCCE_isend_push
#define RCCE_irecv        iRCCE_irecv
#define RCCE_irecv_test   iRCCE_irecv_test
#define RCCE_irecv_wait   iRCCE_irecv_wait
#define RCCE_irecv_push   iRCCE_irecv_push
#define RCCE_SEND_REQUEST iRCCE_SEND_REQUEST
#define RCCE_RECV_REQUEST iRCCE_RECV_REQUEST
#endif
///////////////////////////////////////////////////////////////

#endif

