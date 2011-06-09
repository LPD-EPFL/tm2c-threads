
#include <stdio.h>
#include "stddef.h"
#include "../include/iRCCE.h"

#define ME              printf("[%02d] ", RCCE_ue());
#define PRINTD(args...) ME; printf(args); printf("\n"); fflush(stdout)
#define BARRIER         RCCE_barrier(&RCCE_COMM_WORLD);
#define ONCE            if (RCCE_ue() == 1)

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
        shmem_start_address = (unsigned int) start;
        RCCE_shfree((volatile unsigned char *) start);
    }

    bank_t * bank = (bank_t *) RCCE_shmalloc(sizeof (bank_t));
    if (bank == NULL) {
        PRINTD("bank null");
        exit(1);
    }

    bank->accounts = (account_t *) RCCE_shmalloc(NBACC * sizeof (account_t));
    if (bank->accounts == NULL) {
        PRINTD("bank->accounts null");
        exit(1);
    }

    ONCE
    {
        PRINTD("setting bank->size %d", NBACC);
        bank->size = NBACC;
        int i;
        for (i = 0; i < bank->size; i++) {
            bank->accounts[i].number = i;
            bank->accounts[i].balance = 0;
        }
    }
    BARRIER

    PRINTD("bank->size = %d", bank->size);
    int i;
    if (RCCE_ue()) {
        for (i = 0; i < bank->size; i++);
    }
    else {
        for (i = 0; i < bank->size; i++);
    }

    BARRIER

    RCCE_shfree((t_vcharp) bank->accounts);
    RCCE_shfree((t_vcharp) bank);
    RCCE_finalize();
    return 0;
}
