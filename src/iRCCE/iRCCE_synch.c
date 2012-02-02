///*************************************************************************************
// Synchronization functions. 
// Single-bit and whole-cache-line flags are sufficiently different that we provide
// separate implementations of the synchronization routines for each case
//**************************************************************************************
//
// Author: Rob F. Van der Wijngaart
//         Intel Corporation
// Date:   008/30/2010
//
//**************************************************************************************
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
//    [2011-01-21] updated the datatype of RCCE_FLAG according to the
//                 recent version of RCCE
//
#include "iRCCE_lib.h"

#ifdef SINGLEBITFLAGS

int iRCCE_test_flag(RCCE_FLAG flag, RCCE_FLAG_STATUS val, int *result) {
	t_vcharp cflag;
	t_vcharp flaga;

	cflag = flag.line_address;
	flaga = flag.flag_addr;

	// always flush/invalidate to ensure we read the most recent value of *flag
	// keep reading it until it has the required value 

#ifdef _OPENMP
#pragma omp flush  
#endif
	RC_cache_invalidate();

	if(RCCE_bit_value(flaga, (flag.location)%RCCE_FLAGS_PER_BYTE) != val) {
		(*result) = 0;
	}    
	else {
		(*result) = 1;
	}

	return(iRCCE_SUCCESS);
} 

#else

//////////////////////////////////////////////////////////////////
// LOCKLESS SYNCHRONIZATION USING ONE WHOLE CACHE LINE PER FLAG //
//////////////////////////////////////////////////////////////////

int iRCCE_test_flag(RCCE_FLAG flag, RCCE_FLAG_STATUS val, int *result) {

	t_vcharp flaga = flag.flag_addr;

	// always flush/invalidate to ensure we read the most recent value of *flag
	// keep reading it until it has the required value. We only need to read the
	// first int of the MPB cache line containing the flag
#ifdef _OPENMP
#pragma omp flush   
#endif
	RC_cache_invalidate();

	if((RCCE_FLAG_STATUS)(*flaga) != val) {
		(*result) = 0;
	}    
	else {
		(*result) = 1;
	}

	return(iRCCE_SUCCESS);
}

#endif

