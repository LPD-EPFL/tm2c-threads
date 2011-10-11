/*
 * File:
 *   bank.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Bank stress test.
 *
 * Copyright (c) 2007-2011.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "../include/tm.h"
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>

#ifdef DSL
#undef DSL
#undef ONCE
#define ONCE                            if (RCCE_ue() == 0 || RCCE_num_ues() == 1)
#endif

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */

/*use TX_LOAD_STORE*/
#define LOAD_STORE

/*take advante all 4 MCs*/
#define MC_

#define DEFAULT_DURATION                10
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                20
#define DEFAULT_WRITE_ALL               0
#define DEFAULT_READ_THREADS            0
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_LOCKS                   0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define CAST_VOIDP(addr)                ((void *) (addr))

#define MB16            16777216
#define INDEX(i)        ((((i) % 4) * MB16) + (i))

#ifdef MC
#define I(i)            INDEX(i)
#else
#define I(i)            i
#endif

#define ACCNT(bank, i)  ((account_t *) (bank + 1 + i))
#define ACCP(i)          ACCNT(bank, i)
#define ACC(i)          (*ACCNT(bank, i))


/* ################################################################### *
 * GLOBALS
 * ################################################################### */

/* ################################################################### *
 * BANK ACCOUNTS
 * ################################################################### */

typedef struct account {
    int number;
    int balance;
} account_t;

typedef struct bank {
    account_t *accounts;
    int size;
} bank_t;

inline int getlocknum(int account_num) {
    return (account_num % RCCE_num_ues());
}

void lock_bank() {
    RCCE_acquire_lock(0);
}

void release_lock_bank() {
    RCCE_release_lock(0);
}

int transfer(account_t *src, account_t *dst, int amount, int use_locks) {
    // PRINT("in transfer");

    int i, j;

    if (!use_locks) {
        i = src->balance - amount;
        src->balance = i;
        j = dst->balance + amount;
        dst->balance = j;
    }
    else {
        PRINTD("locking bank");
        lock_bank();
        PRINTD("locked bank");

        src->balance -= amount;
        dst->balance += amount;

        PRINTD("releasing bank");
        release_lock_bank();
        PRINTD("released bank");

    }

    return amount;
}

int total(bank_t *bank, int use_locks) {
    int i, total;

    if (!use_locks) {
        total = 0;
        for (i = 0; i < bank->size; i++) {
            total += bank->accounts[I(i)].balance;
            //total += ACC(I(i)).balance;
        }
    }
    else {
        lock_bank();

        total = 0;
        for (i = 0; i < bank->size; i++) {
            total += bank->accounts[I(i)].balance;
            //total += ACC(I(i)).balance;
        }

        release_lock_bank();


    }

    return total;
}

void reset(bank_t *bank) {
    int i, j = 0;

    TX_START
    for (i = 0; i < bank->size; i++) {
        TX_STORE(&bank->accounts[I(i)].balance, &j, TYPE_INT);
    }
    TX_COMMIT
}

/* ################################################################### *
 * RANDOM
 * ################################################################### */

/* 
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 */
inline long rand_range(long r) {
    int m = RAND_MAX;
    long d, v = 0;

    do {
        d = (m > r ? r : m);
        v += 1 + (long) (d * ((double) rand() / ((double) (m) + 1.0)));
        r -= m;
    } while (r > 0);
    return v;
}

/*
 * Seeding the rand()
 */
inline void srand_core() {
    double timed_ = RCCE_wtime();
    unsigned int timeprfx_ = (unsigned int) timed_;
    unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
    srand(time_ + (13 * (RCCE_ue() + 1)));
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data {
    bank_t *bank;
    unsigned long nb_transfer;
    unsigned long nb_read_all;
    unsigned long nb_write_all;
    unsigned long max_retries;
    int id;
    int read_all;
    int read_cores;
    int write_all;
    int write_cores;
    int use_locks;
    int nb_app_cores;
    char padding[64];
} thread_data_t;

double _duration;

bank_t * test(void *data, double duration, int nb_accounts) {
    int src, dst, nb;
    int rand_max, rand_min;
    thread_data_t *d = (thread_data_t *) data;
    bank_t * bank;


    /* Initialize seed (use randRCCE_num_ues(); as rand is poor) */
    srand_core();

    rand_max = nb_accounts;
    rand_min = 0;

    bank_t **btmp = (bank_t **) RCCE_shmalloc(RCCE_num_ues() * sizeof (bank_t));
    //bank = (bank_t *) RCCE_shmalloc(sizeof (bank_t));
    bank = btmp[RCCE_ue()];
    //bank = (bank_t *) malloc(sizeof (bank_t));
    if (bank == NULL) {
        PRINT("malloc bank");
        EXIT(1);
    }




#ifdef MC
    bank->accounts = (account_t *) RCCE_shmalloc(64 * 1024 * 1024);
#else
    bank->accounts = (account_t *) RCCE_shmalloc(nb_accounts * sizeof (account_t));
#endif

    if (bank->accounts == NULL) {
        PRINT("malloc bank->accounts");
        EXIT(1);
    }

    bank->size = nb_accounts;
    ONCE
    {
        int i;
        for (i = 0; i < bank->size; i++) {
            //       PRINTN("(s %d)", i);
            bank->accounts[I(i)].number = i;
            bank->accounts[I(i)].balance = 0;
            /*
                        ACC(I(i)).number = i;
                        ACC(I(i)).balance = 0;
             */
        }
    }

    ONCE
    {
        PRINT("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\tBank total (before): %d\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
                total(bank, 0));
    }

    BARRIERW

    PRINT("bank size: %d", bank->size);

    BARRIERW

    /* Wait on barrier */

    // PRINT("chk %d", chk++); //0

    FOR(duration) {
        //PRINT("here - -  --");
        if (d->id < d->read_cores) {
            /* Read all */
            //PRINT("READ ALL1");
            total(bank, d->use_locks);
            d->nb_read_all++;
        }
        else if (d->id < d->read_cores + d->write_cores) {
            /* Write all */
            reset(bank);
            d->nb_write_all++;
        }
        else {
            nb = (int) (rand_range(100) - 1);
            if (nb < d->read_all) {
                //PRINT("READ ALL2");
                /* Read all */
                total(bank, d->use_locks);
                d->nb_read_all++;
            }
            else if (nb < d->read_all + d->write_all) {
                /* Write all */
                //PRINT("WRITE ALL");
                reset(bank);
                d->nb_write_all++;
            }
            else {
                //PRINT("Transfer");
                /* Choose random accounts */
                src = (int) (rand_range(rand_max) - 1) + rand_min;
                assert(src < (rand_max + rand_min));
                assert(src >= 0);
                dst = (int) (rand_range(rand_max) - 1) + rand_min;
                assert(dst < (rand_max + rand_min));
                assert(dst >= 0);
                if (dst == src)
                    dst = ((src + 1) % rand_max) + rand_min;
                transfer(&bank->accounts[I(src)], &bank->accounts[I(dst)], 1, d->use_locks);
                //transfer(ACCP(I(src)), ACCP(dst), 1, d->use_locks);

                d->nb_transfer++;
            }
        }
    }

    _duration = duration__;

    //reset(bank);

    PRINT("~~");
    /* Free transaction */

    return bank;
}

TASKMAIN(int argc, char **argv) {
    dup2(STDOUT_FILENO, STDERR_FILENO);

    RCCE_init(&argc, &argv);
    iRCCE_init();

    struct option long_options[] = {
        // These options don't set a flag
        {"help", no_argument, NULL, 'h'},
        {"accounts", required_argument, NULL, 'a'},
        {"contention-manager", required_argument, NULL, 'c'},
        {"duration", required_argument, NULL, 'd'},
        {"num-threads", required_argument, NULL, 'n'},
        {"read-all-rate", required_argument, NULL, 'r'},
        {"read-threads", required_argument, NULL, 'R'},
        {"write-all-rate", required_argument, NULL, 'w'},
        {"write-threads", required_argument, NULL, 'W'},
        {"locks", no_argument, NULL, 'l'},
        {NULL, 0, NULL, 0}
    };

    bank_t *bank;
    int i, c;
    thread_data_t *data;

    double duration = DEFAULT_DURATION;
    int nb_accounts = DEFAULT_NB_ACCOUNTS;
    int nb_app_cores = RCCE_num_ues();
    int read_all = DEFAULT_READ_ALL;
    int read_cores = DEFAULT_READ_THREADS;
    int write_all = DEFAULT_WRITE_ALL;
    int write_cores = DEFAULT_WRITE_THREADS;
    int use_locks = DEFAULT_LOCKS;


    while (1) {
        i = 0;
        c = getopt_long(argc, argv, "ha:d:r:R:w:W:l", long_options, &i);

        if (c == -1)
            break;

        if (c == 0 && long_options[i].flag == 0)
            c = long_options[i].val;

        switch (c) {
            case 0:
                /* Flag is automatically set */
                break;
            case 'h':
                ONCE
            {
                PRINT("bank -- STM stress test\n"
                        "\n"
                        "Usage:\n"
                        "  bank [options...]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help\n"
                        "        Print this message\n"
                        "  -a, --accounts <int>\n"
                        "        Number of accounts in the bank (default=" XSTR(DEFAULT_NB_ACCOUNTS) ")\n"
                        "  -d, --duration <double>\n"
                        "        Test duration in seconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -r, --read-all-rate <int>\n"
                        "        Percentage of read-all transactions (default=" XSTR(DEFAULT_READ_ALL) ")\n"
                        "  -R, --read-threads <int>\n"
                        "        Number of threads issuing only read-all transactions (default=" XSTR(DEFAULT_READ_THREADS) ")\n"
                        "  -w, --write-all-rate <int>\n"
                        "        Percentage of write-all transactions (default=" XSTR(DEFAULT_WRITE_ALL) ")\n"
                        "  -W, --write-threads <int>\n"
                        "        Number of threads issuing only write-all transactions (default=" XSTR(DEFAULT_WRITE_THREADS) ")\n"
                        "  -l, --locks <int>\n"
                        "        To use or not locks (default=" XSTR(DEFAULT_LOCKS) ")\n"
                        );
            }
                exit(0);
            case 'a':
                nb_accounts = atoi(optarg);
                break;
            case 'd':
                duration = atof(optarg);
                break;
            case 'r':
                read_all = atoi(optarg);
                break;
            case 'R':
                read_cores = atoi(optarg);
                break;
            case 'w':
                write_all = atoi(optarg);
                break;
            case 'W':
                write_cores = atoi(optarg);
                break;
            case 'l':
                use_locks = 1;
                break;
            case '?':
                ONCE
            {
                PRINT("Use -h or --help for help\n");
            }

                exit(0);
            default:
                exit(1);
        }
    }


    assert(duration >= 0);
    assert(nb_accounts >= 2);
    assert(nb_app_cores > 0);
    assert(read_all >= 0 && write_all >= 0 && read_all + write_all <= 100);
    assert(read_cores + write_cores <= nb_app_cores);

    ONCE
    {
        PRINTN("BANK sequential");
        if (use_locks) {
            PRINTN("            using locks");
        }
        PRINTN("Nb accounts    : %d\n", nb_accounts);
        PRINTN("Duration       : %fs\n", duration);
        PRINTN("Nb cores       : %d\n", nb_app_cores);
        PRINTN("Read-all rate  : %d\n", read_all);
        PRINTN("Read cores     : %d\n", read_cores);
        PRINTN("Write-all rate : %d\n", write_all);
        PRINTN("Write cores    : %d\n", write_cores);
    }

    if ((data = (thread_data_t *) malloc(sizeof (thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }


    /* Init STM */
    BARRIERW



    data->id = RCCE_ue();
    data->read_all = read_all;
    data->read_cores = read_cores;
    data->write_all = write_all;
    data->write_cores = write_cores;
    data->use_locks = use_locks;
    data->nb_app_cores = nb_app_cores;
    data->nb_transfer = 0;
    data->nb_read_all = 0;
    data->nb_write_all = 0;
    data->max_retries = 0;

    BARRIERW

    bank = test(data, duration, nb_accounts);

    BARRIERW

    printf("---Core %d\n", RCCE_ue());
    printf("  #transfer   : %lu\n", data->nb_transfer);
    printf("  #read-all   : %lu\n", data->nb_read_all);
    printf("  #write-all  : %lu\n", data->nb_write_all);
    printf(" -- \n");
    int total_ = data->nb_transfer + data->nb_read_all + data->nb_write_all;
    printf("Duration    : %f\n", _duration);
    printf("Ops         : %d\n", total_);
    printf("Ops/s       : %d\n", (int) (total_ / _duration));
    printf("Latency     : %f\n", _duration / (double) total_);
    FLUSH

    ONCE
    {
        PRINT("\t\t\t~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
                "\t\t\t\tBank total (after): %d\n"
                "\t\t\t~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
                total(bank, 0));
    }

    /* Delete bank and accounts */

    RCCE_shfree((volatile unsigned char *) bank->accounts);
    RCCE_shfree((volatile unsigned char *) bank);

    free(data);

    EXIT(0);
}

