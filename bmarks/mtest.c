#include <stdio.h>
#include "stddef.h"
#include "../include/iRCCE.h"

#define ME              printf("[%02d] ", NODE_ID());
#define PRINTD(args...) ME; printf(args); printf("\n"); fflush(stdout)
#define BARRIER         RCCE_barrier(&RCCE_COMM_WORLD);
#define ONCE            if (NODE_ID() == 1)
#define AO(addr)        shmem_address_offset((void*) addr)

#define NBACC 10

typedef struct account {
    int number;
    int balance;
} account_t;

typedef struct bank {
    account_t *accounts;
    int size;
} bank_t;

unsigned int shmem_start_address;

inline unsigned int shmem_address_offset(void *shmem_address) {
    return ((int) shmem_address) -shmem_start_address;
}

int main(int argc, char **argv) {
    RCCE_init(&argc, &argv);
    iRCCE_init();

    if (NODE_ID() % 2 == 1) {

        if (!shmem_start_address) {
            char *start = (char *) sys_shmalloc(sizeof (char));
            if (start == NULL) {
                PRINTD("shmalloc shmem_init_start_address");
            }
            shmem_start_address = (unsigned int) start;
            sys_shfree((volatile unsigned char *) start);
        }


        bank_t * bank = (bank_t *) malloc(sizeof (bank_t));
        if (bank == NULL) {
            PRINTD("bank null");
            exit(1);
        }

        bank->accounts = (account_t *) sys_shmalloc(NBACC * sizeof (account_t));
        if (bank->accounts == NULL) {
            PRINTD("bank->accounts null");
            exit(1);
        }

        BARRIER


        PRINTD("(1)[start addr: %p] bank->accounts (%p : %d)", (void *) shmem_start_address,
                bank->accounts, AO(bank->accounts));


        BARRIER

        bank->size = NBACC;
        
        ONCE
        {
            
            PRINTD("set bank->size %d", bank->size);
            int i;
            for (i = 0; i < bank->size; i++) {
                bank->accounts[i].number = i;
                bank->accounts[i].balance = 542;
            }
            PRINTD("init balances");

        }


        BARRIER


        PRINTD("(2)[start addr: %p] bank->accounts (%p : %d)", (void *) shmem_start_address,
                bank->accounts, AO(bank->accounts));

        /*
                ONCE
                {
                    int i, total = 0;
                    for (i = 0; i < bank->size; i++) {
                        total += bank->accounts[i].balance;
                    }
                    PRINTD("Total: %d", total);
                }
         */



        BARRIER

                int index = 0;
        PRINTD("bank->size = %d", bank->size);
        
        BARRIER
                
        PRINTD("bank->accounts[%d].balance = %d", index, bank->accounts[index].balance);

        /*
                PRINTD("bank->size = %d", bank->size);

                int l = 2;
                while (l--) {
                    PRINTD("round %2d", l);
                    int i;
                    if (NODE_ID() == 1) {
                        for (i = 0; i < bank->size; i++) {
                            bank->accounts[i].balance = i;
                        }
                    }
                    else {
                        int total = 0;
                        for (i = 0; i < bank->size; i++) {      
                                                total += 
                            bank->accounts[i].balance;
                        }
                        PRINTD("-- Total: %d", total);
                    }
                }
         */

        BARRIER

        sys_shfree((t_vcharp) bank->accounts);
        free(bank);
    }
    else {
        BARRIER
        BARRIER
        BARRIER
        BARRIER
    }

    RCCE_finalize();
    return 0;
}
