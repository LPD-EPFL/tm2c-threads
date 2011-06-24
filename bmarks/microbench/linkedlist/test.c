/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses of the linked list
 *
 * Copyright (c) 2009-2010.
 *
 * test.c is part of Microbench
 * 
 * Microbench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "linkedlist.h"
#include <unistd.h>

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

/* Re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
    int m = RAND_MAX;
    long d, v = 0;

    do {
        d = (m > r ? r : m);
        v += 1 + (long) (d * ((double) rand_r(seed) / ((double) (m) + 1.0)));
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

typedef struct thread_data {
    val_t first;
    long range;
    int update;
    int unit_tx;
    int alternate;
    int effective;
    unsigned long nb_add;
    unsigned long nb_added;
    unsigned long nb_remove;
    unsigned long nb_removed;
    unsigned long nb_contains;
    unsigned long nb_found;
    unsigned int seed;
    intset_t *set;
    unsigned long failures_because_contention;
} thread_data_t;

void *test(void *data, double duration) {
    int unext, last = -1;
    val_t val = 0;

    thread_data_t *d = (thread_data_t *) data;

    srand_core();

    /* Create transaction */
    TM_INITs
    /* Wait on barrier */

    /* Is the first op an update? */
    unext = (rand_range(100) - 1 < d->update);

    FOR(duration) {
        if (unext) { // update

            if (last < 0) { // add

                val = rand_range_re(&d->seed, d->range);
                if (set_add(d->set, val, TRANSACTIONAL)) {
                    d->nb_added++;
                    last = val;
                }
                d->nb_add++;

            }
            else { // remove

                if (d->alternate) { // alternate mode (default)
                    if (set_remove(d->set, last, TRANSACTIONAL)) {
                        d->nb_removed++;
                    }
                    last = -1;
                }
                else {
                    /* Random computation only in non-alternated cases */
                    val = rand_range_re(&d->seed, d->range);
                    /* Remove one random value */
                    if (set_remove(d->set, val, TRANSACTIONAL)) {
                        d->nb_removed++;
                        /* Repeat until successful, to avoid size variations */
                        last = -1;
                    }
                }
                d->nb_remove++;
            }
        }
        else { // read

            if (d->alternate) {
                if (d->update == 0) {
                    if (last < 0) {
                        val = d->first;
                        last = val;
                    }
                    else { // last >= 0
                        val = rand_range_re(&d->seed, d->range);
                        last = -1;
                    }
                }
                else { // update != 0
                    if (last < 0) {
                        val = rand_range_re(&d->seed, d->range);
                        //last = val;
                    }
                    else {
                        val = last;
                    }
                }
            }
            else val = rand_range_re(&d->seed, d->range);

            if (set_contains(d->set, val, TRANSACTIONAL))
                d->nb_found++;
            d->nb_contains++;

        }

        /* Is the next op an update? */
        if (d->effective) { // a failed remove/add is a read-only tx
            unext = ((100 * (d->nb_added + d->nb_removed))
                    < (d->update * (d->nb_add + d->nb_remove + d->nb_contains)));
        }
        else { // remove/add (even failed) is considered as an update
            unext = (rand_range_re(&d->seed, 100) - 1 < d->update);
        }
    }

    TM_END_STATS
    BARRIER
            /*
                ONCE
                {
                    set_print(d->set);
                }
                BARRIER
             */
    return NULL;
}

TASKMAIN(int argc, char **argv) {
    dup2(STDOUT_FILENO, STDERR_FILENO);

    RCCE_init(&argc, &argv);
    iRCCE_init();

    struct option long_options[] = {
        // These options don't set a flag
        {"help", no_argument, NULL, 'h'},
        {"duration", required_argument, NULL, 'd'},
        {"initial-size", required_argument, NULL, 'i'},
        {"range", required_argument, NULL, 'r'},
        {"update-rate", required_argument, NULL, 'u'},
        {"elasticity", required_argument, NULL, 'x'},
        {NULL, 0, NULL, 0}
    };

    intset_t *set;
    int i, c, size;
    val_t last = 0;
    val_t val = 0;
    thread_data_t *data;
    double duration = DEFAULT_DURATION;
    int initial = DEFAULT_INITIAL;
#ifdef DSL
    int nb_app_cores = (RCCE_num_ues() / 2) + ((RCCE_num_ues() % 2) ? 1 : 0);
#else
    int nb_app_cores = RCCE_num_ues();
#endif
    long range = DEFAULT_RANGE;
    int update = DEFAULT_UPDATE;
    int unit_tx = DEFAULT_ELASTICITY;
    int alternate = DEFAULT_ALTERNATE;
    int effective = DEFAULT_EFFECTIVE;
    unsigned int seed = 0;

    while (1) {
        i = 0;
        c = getopt_long(argc, argv, "hAf:d:i:r:u:x:", long_options, &i);

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
                printf("intset -- STM stress test "
                        "(linked list)\n"
                        "\n"
                        "Usage:\n"
                        "  intset [options...]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help\n"
                        "        Print this message\n"
                        "  -A, --alternate (default="XSTR(DEFAULT_ALTERNATE)")\n"
                        "        Consecutive insert/remove target the same value\n"
                        "  -f, --effective <int>\n"
                        "        update txs must effectively write (0=trial, 1=effective, default=" XSTR(DEFAULT_EFFECTIVE) ")\n"
                        "  -d, --duration secs<double>\n"
                        "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -i, --initial-size <int>\n"
                        "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
                        "  -r, --range <int>\n"
                        "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
                        "  -u, --update-rate <int>\n"
                        "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
                        "  -x, --elasticity (default=4)\n"
                        "        Use elastic transactions\n"
                        "        0 = non-protected,\n"
                        "        1 = normal transaction,\n"
                        "        2 = read elastic-tx,\n"
                        "        3 = read/add elastic-tx,\n"
                        "        4 = read/add/rem elastic-tx,\n"
                        "        5 = all recursive elastic-tx,\n"
                        "        6 = harris lock-free\n"
                        );
            }
                exit(0);
            case 'A':
                alternate = 1;
                break;
            case 'f':
                effective = atoi(optarg);
                break;
            case 'd':
                duration = atof(optarg);
                break;
            case 'i':
                initial = atoi(optarg);
                break;
            case 'r':
                range = atol(optarg);
                break;
            case 'u':
                update = atoi(optarg);
                break;
            case 'x':
                unit_tx = atoi(optarg);
                break;
            case '?':
                ONCE
            {
                printf("Use -h or --help for help\n");
            }
                exit(0);
            default:
                exit(1);
        }
    }

    if (seed == 0)
        srand((RCCE_ue() + 1) * (int) time(0));
    else
        srand(seed);

    assert(duration >= 0);
    assert(initial >= 0);
    assert(nb_app_cores > 0);
    assert(range > 0 && range >= initial);
    assert(update >= 0 && update <= 100);

    ONCE
    {
        printf("Bench type   : linked list\n");
#ifdef EARLY_RELEASE 
        printf("                using early-release\n");
#endif
        printf("Duration     : %f\n", duration);
        printf("Initial size : %d\n", initial);
        printf("Nb cores     : %d\n", nb_app_cores);
        printf("Value range  : %ld\n", range);
        printf("Update rate  : %d\n", update);
        printf("Elasticity   : %d\n", unit_tx);
        printf("Alternate    : %d\n", alternate);
        printf("Effective    : %d\n", effective);
        FLUSH
    }

    if ((data = (thread_data_t *) malloc(sizeof (thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    set = set_new();

    BARRIERW;

    ONCE
    {
        /* Populate set */
        printf("Adding %d entries to set\n", initial);
        i = 0;
        while (i < initial) {
            val = rand_range(range);
            if (set_add(set, val, 0)) {
                last = val;
                i++;
            }
        }
        size = set_size(set);
        printf("Set size     : %d\n", size);
        /*
                set_print(set);
         */
        assert(size == initial);
        FLUSH
    }

    shmem_init((initial + (RCCE_ue() * 1000)) * sizeof (node_t));

    /* Access set from all threads */
    data->first = last;
    data->range = range;
    data->update = update;
    data->unit_tx = unit_tx;
    data->alternate = alternate;
    data->effective = effective;
    data->nb_add = 0;
    data->nb_added = 0;
    data->nb_remove = 0;
    data->nb_removed = 0;
    data->nb_contains = 0;
    data->nb_found = 0;
    data->set = set;
    data->seed = seed;

    BARRIERW
    /* Start */
    test(data, duration);

    printf("-- Core %d\n", RCCE_ue());
    printf("  #add        : %lu\n", data->nb_add);
    printf("    #added    : %lu\n", data->nb_added);
    printf("  #remove     : %lu\n", data->nb_remove);
    printf("    #removed  : %lu\n", data->nb_removed);
    printf("  #contains   : %lu\n", data->nb_contains);
    printf("    #found    : %lu\n", data->nb_found);
    printf("---------------------------------------------------");
    FLUSH;
    /* Delete set */



    set_delete(set);

    /* Cleanup STM */

    free(data);

    RCCE_finalize();
    EXIT(0);
}
