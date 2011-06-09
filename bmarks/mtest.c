
#include <stdio.h>
#include "stddef.h"
#include "../include/iRCCE.h"

#define PRINTD(args...) ME; printf(args); printf("\n"); fflush(stdout)

#define NBACC 100

typedef struct account {
    int number;
    int balance;
} account_t;

typedef struct bank {
    account_t *accounts;
    int size;
} bank_t;

int main(int argc, char **argv) {
    RCCE_init(&argc, &argv);
    iRCCE_init();
    
    unsigned int shmem_start_address;
    
    if (!shmem_start_address) {
        char *start = (char *) RCCE_shmalloc(sizeof (char));
        if (start == NULL) {
            PRINTD("shmalloc shmem_init_start_address");
        }
        shmem_start_address = (SHMEM_START_ADDRESS) start;
        RCCE_shfree((volatile unsigned char *) start);
        return TRUE;
    }

    bank_t * bank = (bank_t *) RCCE_shmalloc(sizeof(bank_t));
    if (bank == NULL) {
        PRINTD("bank null");
        exit(1);
    }
    
    bank->accounts = (account_t *) RCCE_shmalloc(NBACC * sizeof(account_t));
    if (bank->accounts == NULL) {
        PRINTD("bank->accounts null");
        exit(1);
    }
    
    
    
    RCCE_free((t_vcharp) bank->accounts);
    RCCE_free((t_vcharp) bank);
    RCCE_finalize();
    return 0;
}
