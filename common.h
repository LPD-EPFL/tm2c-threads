#ifndef _COMMON_H_
#define _COMMON_H_

#define P(args...) printf("[%d] ", ID); printf(args); printf("\n"); fflush(stdout)
#define PRINT P

extern int ID;
#endif
