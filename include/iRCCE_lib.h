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
#ifndef IRCCE_LIB_H
#define IRCCE_LIB_H
#include "iRCCE.h"
#include "RCCE_lib.h"

extern iRCCE_SEND_REQUEST* iRCCE_isend_queue;
extern iRCCE_SEND_REQUEST* iRCCE_isend_queue32[RCCE_MAXNP];
extern iRCCE_RECV_REQUEST* iRCCE_irecv_queue[RCCE_MAXNP];
#ifdef _OPENMP
#pragma omp threadprivate (iRCCE_isend_queue, iRCCE_irecv_queue)
#endif

int iRCCE_test_flag(RCCE_FLAG, RCCE_FLAG_STATUS, int *);

#endif

