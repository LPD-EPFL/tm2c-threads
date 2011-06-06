#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "tm.h"

int PAGE_SIZE;

int ReadConfigReg(unsigned int ConfigAddr);

MAIN(int argc, char **argv) {

    RCCE_init(&argc, &argv);
    int ID = RCCE_ue();
    int NUM_UES = RCCE_num_ues();

    PAGE_SIZE = getpagesize();
    
    unsigned int alignedAddr;
    
        alignedAddr = 0xf9000000;
     
  


    unsigned int gc_address_lo = (unsigned int) ReadConfigReg(alignedAddr + 0x8224);
    unsigned int gc_address_up = (unsigned int) ReadConfigReg(alignedAddr + 0x8228);
    
    PRINTD("(up) %u (lo) %u", gc_address_up, gc_address_lo);

    RCCE_finalize();
    EXIT(0);
}
