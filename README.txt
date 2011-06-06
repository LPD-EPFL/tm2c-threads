//
// Copyright 2010  Chair for Operating Systems,
//                 RWTH Aachen University
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

=======================================================================
 Welcome to iRCCE, an extension to the RCCE communication environment:
=======================================================================

In case of any problems, questions or ideas for improvements, 
do not hesitate to contact us: contact@lfbs.rwth-aachen.de

-----------------------
 Installation:
-----------------------

1. Build the RCCE library. See README in the "rcce" folder for
   configuration/building instructions.

2. Change to the iRCCE directory and type "./configure <Path/To/RCCE>".
   This checks for the file "common/symbols" in the RCCE tree.

3. Type "make" for building the iRCCE extensions, which are directly
   installed into the RCCE library.

4. Change to the "apps" folder of iRCCE and type "make all" for testing.


-----------------------
 Usage / API:
-----------------------

* Initialization function:
- int   iRCCE_init(void);

* Non-blocking send/recv functions:
- int   iRCCE_isend(char *buffer, size_t length, int dest, iRCCE_SEND_REQUEST *send_request);
- int   iRCCE_isend_test(iRCCE_SEND_REQUEST *send_request, int *test_flag);
- int   iRCCE_isend_wait(iRCCE_SEND_REQUEST *send_request);
- int   iRCCE_isend_push(void);
- int   iRCCE_irecv(char *buffer, size_t length, int source, iRCCE_RECV_REQUEST *recv_request);
- int   iRCCE_irecv_test(iRCCE_RECV_REQUEST *recv_request, int *test_flag);
- int   iRCCE_irecv_wait(iRCCE_RECV_REQUEST *recv_request);
- int   iRCCE_irecv_push(void);

* Blocking but _pipelined_ send/recv functions:
- int   iRCCE_send(char *buffer, size_t length, int dest);
- int   iRCCE_recv(char *buffer, size_t length, int source);

* SCC-customized put/get and memcpy functions:
- int   iRCCE_put(t_vcharp target, t_vcharp source, int length, int ID);
- int   iRCCE_get(t_vcharp target, t_vcharp source, int length, int ID);
- void* iRCCE_memcpy_put(void* dest, const void* src, size_t count);
- void* iRCCE_memcpy_get(void* dest, const void* src, size_t count);

* Cancel functions for yet not started non-blocking requests:
- int   iRCCE_isend_cancel(iRCCE_SEND_REQUEST *send_request, int *test_flag);
- int   iRCCE_irecv_cancel(iRCCE_RECV_REQUEST *recv_request, int *test_flag);

* Wait/Test-all/any functions:
- void  iRCCE_init_wait_list(iRCCE_WAIT_LIST *wait_list);
- void  iRCCE_add_to_wait_list(iRCCE_WAIT_LIST *wait_list, iRCCE_SEND_REQUEST *send_request, iRCCE_RECV_REQUEST *recv_request);
- int   iRCCE_test_all(iRCCE_WAIT_LIST *wait_list, int *test_flag);
- int   iRCCE_wait_all(iRCCE_WAIT_LIST *wait_list);
- int   iRCCE_test_any(iRCCE_WAIT_LIST *wait_list, iRCCE_SEND_REQUEST **send_request, iRCCE_RECV_REQUEST **recv_request);
- int   iRCCE_wait_any(iRCCE_WAIT_LIST *wait_list, iRCCE_SEND_REQUEST **send_request, iRCCE_RECV_REQUEST **recv_request);

... see the "iRCCE Manual" for more detailed information about the iRCCE API!


=======================================================================
 Some Explanatory Notes by Carsten Scholtes (Uni Bayreuth):
=======================================================================

-----------------------
 Caution:
-----------------------

1. No blocking send/recv between isend/irecv and corresponding wait.
  (May interfere with signals and MPBs already in use
  by requests queued earlier.) [--> should be fixed yet!]
2. No new flags or other MPB allocations between isend/irecv and wait.
  (Amount and position of MPB space usable for data transfer is recorded
  at creation of request (request->chunk, request->combuf)
  and used throughout request completion.)
3. Congruent MPB allocations in all processes expected.
  (Addresses (of flags and data) in other processes' MPBs
  are computed under this assumption.)


-----------------------
 Algorithm / Strategy:
-----------------------

Queue asynchronous requests until wait (if not immediately serviceable).
No extra thread communicating in the background.
Using busy waits (like blocking operations).
(Ok on SCC, since no other asynchronous hardware available...)

