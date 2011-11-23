/* 
 * File:   common.h
 * Author: trigonak
 *
 * Created on March 30, 2011, 6:15 PM
 */

#ifndef COMMON_H
#define	COMMON_H

#ifdef	__cplusplus
extern "C" {
#endif

#define DSL

#define PGAS
#define READ_BUF_OFF

#define DSLNDPERNODES   2 /* 1 dedicated DS-Locking core per DSLNDPERNODES cores*/
#define NUM_DSL_UES     ((int) ((RCCE_num_ues() / DSLNDPERNODES)) + (RCCE_num_ues() % DSLNDPERNODES ? 1 : 0))
#define NUM_APP_UES     (RCCE_num_ues() - NUM_DSL_UES)
    extern unsigned int NUM_DSL_NODES;

#define DEBUG_

#define MED printf("[%02d] ", RCCE_ue());
#define PRINT(args...) printf("[%02d] ", RCCE_ue()); printf(args); printf("\n"); fflush(stdout)
#define PRINTN(args...) printf("[%02d] ", RCCE_ue()); printf(args); fflush(stdout)
#define PRINTS(args...)  printf(args);
#define PRINTSF(args...)  printf(args); fflush(stdout)
#define PRINTSME(args...)  printf("[%02d] ", RCCE_ue()); printf(args);

#define BMSTART(what) {const char *__bchm_target = what; double __start_time = RCCE_wtime();
#define BMEND double __end_time = RCCE_wtime(); ME; printf("[benchmarking] "); printf("%s", __bchm_target);\
        printf(" | %f secs\n", __end_time - __start_time); fflush(stdout);}


#define FLUSH fflush(stdout);
#ifdef DEBUG
#define FLUSHD fflush(stdout);
#define ME printf("%d: ", RCCE_ue())
#define PRINTF(args...) printf(args)
#define PRINTD(args...) ME; printf(args); printf("\n"); fflush(stdout)
#define PRINTDNN(args...) ME; printf(args); fflush(stdout)
#define PRINTD1(UE, args...) if(RCCE_ue() == (UE)) { ME; printf(args); printf("\n"); fflush(stdout); }
#define TS printf("[%f] ", RCCE_wtime())
#else
#define FLUSHD
#define ME
#define PRINTF(args...)
#define PRINTD(args...)
#define PRINTDNN(args...)
#define PRINTD1(UE, args...)
#define TS
#endif

    typedef enum {
        FALSE, //0
        TRUE //1
    } BOOLEAN;

    typedef enum {
        NO_CONFLICT,
        READ_AFTER_WRITE,
        WRITE_AFTER_READ,
        WRITE_AFTER_WRITE
    } CONFLICT_TYPE;

    /* read or write request
     */
    typedef enum {
        READ,
        WRITE
    } RW;

    extern unsigned int ID; //=RCCE_ue()
    extern unsigned int NUM_UES;

#include "iRCCE.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include "measurements.h"

#define TASKMAIN MAIN
#define MAIN int main
#define EXIT(reason) exit(reason);
#define EXITALL(reason) exit((reason))

#define BARRIER RCCE_barrier(&RCCE_COMM_APP);
#define BARRIERW RCCE_barrier(&RCCE_COMM_WORLD);

#ifdef	__cplusplus
}
#endif

#endif	/* COMMON_H */

