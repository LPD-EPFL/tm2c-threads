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

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */

#define DEFAULT_DURATION                10
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                20
#define DEFAULT_WRITE_ALL               0
#define DEFAULT_READ_THREADS            0
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s
#define FOR(seconds)                    double starting__ = RCCE_wtime(), duration__;\
                                            while ((duration__ =\
                                            (RCCE_wtime() - starting__)) < (seconds))
#define ONCE                            if (RCCE_ue() == 1)

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

int transfer(account_t *src, account_t *dst, int amount) {
    int i, j;

    /* Allow overdrafts */
    TX_START
    i = (int) TX_LOAD(&src->balance);
    i -= amount;
    TX_STORE(&src->balance, &i, TYPE_INT); //NEED TX_STOREI
    j = (int) TX_LOAD(&dst->balance);
    j += amount;
    TX_STORE(&dst->balance, &j, TYPE_INT);
    TX_COMMIT

    return amount;
}

int total(bank_t *bank, int transactional) {
    int i, total;

    if (!transactional) {
        total = 0;
        for (i = 0; i < bank->size; i++) {
            total += bank->accounts[i].balance;
        }
    }
    else {
        TX_START
        total = 0;
        for (i = 0; i < bank->size; i++) {
            total += (int) TX_LOAD(&bank->accounts[i].balance);
        }
        TX_COMMIT;
    }

    return total;
}

void reset(bank_t *bank) {
    int i, j = 0;

    TX_START
    for (i = 0; i < bank->size; i++) {
        TX_STORE(&bank->accounts[i].balance, &j, TYPE_INT);
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
    unsigned long nb_aborts;
    unsigned long nb_aborts_1;
    unsigned long nb_aborts_2;
    unsigned long nb_aborts_locked_read;
    unsigned long nb_aborts_locked_write;
    unsigned long nb_aborts_validate_read;
    unsigned long nb_aborts_validate_write;
    unsigned long nb_aborts_validate_commit;
    unsigned long nb_aborts_invalid_memory;
    unsigned long nb_aborts_killed;
    unsigned long locked_reads_ok;
    unsigned long locked_reads_failed;
    unsigned long max_retries;
    unsigned int seed;
    int id;
    int read_all;
    int read_cores;
    int write_all;
    int write_cores;
    int disjoint;
    int nb_app_cores;
    char padding[64];
} thread_data_t;

void test(void *data, double duration) {
    int src, dst, nb;
    int rand_max, rand_min;
    thread_data_t *d = (thread_data_t *) data;

    /* Initialize seed (use rand48 as rand is poor) */
    srand_core();

    /* Prepare for disjoint access */
    if (d->disjoint) {
        rand_max = d->bank->size / d->nb_app_cores;
        rand_min = rand_max * d->id;
        if (rand_max <= 2) {
            fprintf(stderr, "can't have disjoint account accesses");
            return;
        }
    }
    else {
        rand_max = d->bank->size;
        rand_min = 0;
    }

    /* Create transaction */
    TM_INITs
    /* Wait on barrier */

    FOR(duration) {
        if (d->id < d->read_cores) {
            /* Read all */
            total(d->bank, 1);
            d->nb_read_all++;
        }
        else if (d->id < d->read_cores + d->write_cores) {
            /* Write all */
            reset(d->bank);
            d->nb_write_all++;
        }
        else {
            nb = (int) (rand_range(100) - 1);
            if (nb < d->read_all) {
                /* Read all */
                total(d->bank, 1);
                d->nb_read_all++;
            }
            else if (nb < d->read_all + d->write_all) {
                /* Write all */
                reset(d->bank);
                d->nb_write_all++;
            }
            else {
                /* Choose random accounts */
                src = (int) (rand_range(rand_max + 1) - 1) + rand_min;
                assert(src < rand_max);
                assert(src >= 0);
                dst = (int) (rand_range(rand_max + 1) - 1) + rand_min;
                assert(dst < rand_max);
                assert(dst >= 0);
                if (dst == src)
                    dst = ((src + 1) % rand_max) + rand_min;
                printf("Transfering: [%5d] (%d) to [%5d] (%d) | ", src, d->bank->accounts[src].balance, dst, d->bank->accounts[dst].balance);
                transfer(&d->bank->accounts[src], &d->bank->accounts[dst], 1);
                printf("After: [%5d] (%d) - [%5d] (%d)\n", src, d->bank->accounts[src].balance, dst, d->bank->accounts[dst].balance);
                d->nb_transfer++;
            }
        }
    }

    //reset(d->bank);

    BARRIER
    /* Free transaction */
    TM_END_STATS

    return;
}

void catch_function(int signal) {
    PRINT("SIGABRT");
    EXIT(1);
}

inline void assign_abort_sig_handler() {
    struct sigaction action;
    action.sa_handler = catch_function;
    action.sa_flags = 0;

    if (sigaction(SIGABRT, &action, NULL) == -1) {
        PRINTD("sigaction @ assign_abort_sig_handler");
        //TODO: cleanup
        EXIT(-1);
    }
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
        {"disjoint", no_argument, NULL, 'j'},
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
    int disjoint = DEFAULT_DISJOINT;

    while (1) {
        i = 0;
        c = getopt_long(argc, argv, "ha:d:r:R:w:W:j", long_options, &i);

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
                        "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -r, --read-all-rate <int>\n"
                        "        Percentage of read-all transactions (default=" XSTR(DEFAULT_READ_ALL) ")\n"
                        "  -R, --read-threads <int>\n"
                        "        Number of threads issuing only read-all transactions (default=" XSTR(DEFAULT_READ_THREADS) ")\n"
                        "  -w, --write-all-rate <int>\n"
                        "        Percentage of write-all transactions (default=" XSTR(DEFAULT_WRITE_ALL) ")\n"
                        "  -W, --write-threads <int>\n"
                        "        Number of threads issuing only write-all transactions (default=" XSTR(DEFAULT_WRITE_THREADS) ")\n"
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
            case 'j':
                disjoint = 1;
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
        PRINTN("Nb accounts    : %d\n", nb_accounts);
        PRINTN("Duration       : %f\n", duration);
        PRINTN("Nb threads     : %d\n", nb_app_cores);
        PRINTN("Read-all rate  : %d\n", read_all);
        PRINTN("Read threads   : %d\n", read_cores);
        PRINTN("Write-all rate : %d\n", write_all);
        PRINTN("Write threads  : %d\n", write_cores);
        PRINTN("Type sizes     : int=%d/long=%d/ptr=%d\n",
                (int) sizeof (int),
                (int) sizeof (long),
                (int) sizeof (void *)
                );
    }
    if ((data = (thread_data_t *) malloc(sizeof (thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    bank = (bank_t *) RCCE_shmalloc(sizeof (bank_t));
    ONCE
    {
        bank->accounts = (account_t *) RCCE_shmalloc(nb_accounts * sizeof (account_t));
        bank->size = nb_accounts;
        for (i = 0; i < bank->size; i++) {
            bank->accounts[i].number = i;
            bank->accounts[i].balance = 0;
        }
    }

    /* Init STM */
    BARRIERW

    data->id = i;
    data->read_all = read_all;
    data->read_cores = read_cores;
    data->write_all = write_all;
    data->write_cores = write_cores;
    data->disjoint = disjoint;
    data->nb_app_cores = nb_app_cores;
    data->nb_transfer = 0;
    data->nb_read_all = 0;
    data->nb_write_all = 0;
    data->nb_aborts = 0;
    data->nb_aborts_1 = 0;
    data->nb_aborts_2 = 0;
    data->nb_aborts_locked_read = 0;
    data->nb_aborts_locked_write = 0;
    data->nb_aborts_validate_read = 0;
    data->nb_aborts_validate_write = 0;
    data->nb_aborts_validate_commit = 0;
    data->nb_aborts_invalid_memory = 0;
    data->nb_aborts_killed = 0;
    data->locked_reads_ok = 0;
    data->locked_reads_failed = 0;
    data->max_retries = 0;
    data->bank = bank;

    ONCE
    {
        PRINT("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\tBank total (before): %d\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
                total(data->bank, 0));
    }

    test(data, duration);

    printf("Core %d\n", RCCE_ue());
    printf("  #transfer   : %lu\n", data->nb_transfer);
    printf("  #read-all   : %lu\n", data->nb_read_all);
    printf("  #write-all  : %lu\n", data->nb_write_all);
    printf("  #aborts     : %lu\n", data->nb_aborts);
    FLUSH

    ONCE
    {
        PRINT("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\tBank total (after): %d\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
                total(data->bank, 0));
    }

    /* Delete bank and accounts */

    RCCE_shfree((volatile unsigned char *) bank->accounts);
    RCCE_shfree((volatile unsigned char *) bank);

    free(data);

    EXIT(0);
}

