/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses of a hashtable
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

#include "intset.h"

#ifdef SEQUENTIAL
#ifdef BARRIER
#undef BARRIER
#define BARRIER BARRIERW
#endif
#endif

/* Hashtable length (# of buckets) */
unsigned int maxhtlength;

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
    int move;
    int snapshot;
    int unit_tx;
    int alternate;
    int effective;
    unsigned long nb_add;
    unsigned long nb_added;
    unsigned long nb_remove;
    unsigned long nb_removed;
    unsigned long nb_contains;
    /* added for HashTables */
    unsigned long load_factor;
    unsigned long nb_move;
    unsigned long nb_moved;
    unsigned long nb_snapshot;
    unsigned long nb_snapshoted;
    /* end: added for HashTables */
    unsigned long nb_found;
    unsigned int seed;
    ht_intset_t *set;
} thread_data_t;

void *test(void *data, double duration) {
    int val2, numtx, r, last = -1;
    val_t val = 0;
    int unext, mnext, cnext;

    thread_data_t *d = (thread_data_t *) data;

    srand_core();

    /* Is the first op an update, a move? */
    r = rand_range_re(&d->seed, 100) - 1;
    unext = (r < d->update);
    mnext = (r < d->move);
    cnext = (r >= d->update + d->snapshot);

    FOR(duration) {

        if (unext) { // update

            if (mnext) { // move

                if (last == -1) val = rand_range_re(&d->seed, d->range);
                else val = last;
                val2 = rand_range_re(&d->seed, d->range);
                if (ht_move(d->set, val, val2, TRANSACTIONAL)) {
                    d->nb_moved++;
                    last = -1;
                }
                d->nb_move++;

            }
            else if (last < 0) { // add

                val = rand_range_re(&d->seed, d->range);
                if (ht_add(d->set, val, TRANSACTIONAL)) {
                    d->nb_added++;
                    last = val;
                }
                d->nb_add++;

            }
            else { // remove

                if (d->alternate) { // alternate mode
                    if (ht_remove(d->set, last, TRANSACTIONAL)) {
                        d->nb_removed++;
                        last = -1;
                    }
                }
                else {
                    /* Random computation only in non-alternated cases */
                    val = rand_range_re(&d->seed, d->range);
                    /* Remove one random value */
                    if (ht_remove(d->set, val, TRANSACTIONAL)) {
                        d->nb_removed++;
                        /* Repeat until successful, to avoid size variations */
                        last = -1;
                    }
                }
                d->nb_remove++;
            }

        }
        else { // reads

            if (cnext) { // contains (no snapshot)

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

                if (ht_contains(d->set, val, TRANSACTIONAL))
                    d->nb_found++;
                d->nb_contains++;

            }
            else { // snapshot

                if (ht_snapshot(d->set, TRANSACTIONAL))
                    d->nb_snapshoted++;
                d->nb_snapshot++;

            }
        }

        /* Is the next op an update, a move, a contains? */
        if (d->effective) { // a failed remove/add is a read-only tx
            numtx = d->nb_contains + d->nb_add + d->nb_remove + d->nb_move + d->nb_snapshot;
            unext = ((100.0 * (d->nb_added + d->nb_removed + d->nb_moved)) < (d->update * numtx));
            mnext = ((100.0 * d->nb_moved) < (d->move * numtx));
            cnext = !((100.0 * d->nb_snapshoted) < (d->snapshot * numtx));
        }
        else { // remove/add (even failed) is considered as an update
            r = rand_range_re(&d->seed, 100) - 1;
            unext = (r < d->update);
            mnext = (r < d->move);
            cnext = (r >= d->update + d->snapshot);
        }

    }

    /* Free transaction */

    return NULL;
}

void *test2(void *data, double duration) {
    int val, newval, last, flag = 1;
    thread_data_t *d = (thread_data_t *) data;

    srand_core();

    /* Create transaction */
    /* Wait on barrier */

    last = 0; // to avoid warning

    val = rand_range_re(&d->seed, 100) - 1;

    /* added for HashTables */

    FOR(duration) {
        if (val < d->update) {
            if (val >= d->move) { /* update without move */
                if (flag) {
                    /* Add random value */
                    val = (rand_r(&d->seed) % d->range) + 1;
                    if (ht_add(d->set, val, TRANSACTIONAL)) {
                        d->nb_added++;
                        last = val;
                        flag = 0;
                    }
                    d->nb_add++;
                }
                else {
                    if (d->alternate) {
                        /* Remove last value */
                        if (ht_remove(d->set, last, TRANSACTIONAL))
                            d->nb_removed++;
                        d->nb_remove++;
                        flag = 1;
                    }
                    else {
                        /* Random computation only in non-alternated cases */
                        newval = rand_range_re(&d->seed, d->range);
                        if (ht_remove(d->set, newval, TRANSACTIONAL)) {
                            d->nb_removed++;
                            /* Repeat until successful, to avoid size variations */
                            flag = 1;
                        }
                        d->nb_remove++;
                    }
                }
            }
            else { /* move */
                val = rand_range_re(&d->seed, d->range);
                if (ht_move(d->set, last, val, TRANSACTIONAL)) {
                    d->nb_moved++;
                    last = val;
                }
                d->nb_move++;
            }
        }
        else {
            if (val >= d->update + d->snapshot) { /* read-only without snapshot */
                /* Look for random value */
                val = rand_range_re(&d->seed, d->range);
                if (ht_contains(d->set, val, TRANSACTIONAL))
                    d->nb_found++;
                d->nb_contains++;
            }
            else { /* snapshot */
                if (ht_snapshot(d->set, TRANSACTIONAL))
                    d->nb_snapshoted++;
                d->nb_snapshot++;
            }
        }
    }

    /* Free transaction */
    return NULL;
}

void print_set(intset_t* set) {

    TX_START
    node_t node = (node_t) TX_LOAD(set->head);

    if (node.next == NULL) {
        goto null;
    }
    while (node.next != NULL) {
        //printf("{%d} -%d-> ", node.val, node.next);
        printf("%5d =(%3d)=> ", node.val, node.next);
        node = (node_t) TX_LOAD(node.next);
    }
    TX_COMMIT
    null :
            PRINTSF("NULL\n");
}

void print_ht(ht_intset_t *set) {
    printf("HASHTABLE (%d buckets)------------------------------------------------------------\n", maxhtlength);
    int i;
    for (i = 0; i < maxhtlength; i++) {
        printf("[%3d] :: ", i);
        print_set(set->buckets[i]);
    }
}

TASKMAIN(int argc, char **argv) {
    TM_INIT

            struct option long_options[] = {
        // These options don't set a flag
        {"help", no_argument, NULL, 'h'},
        {"duration", required_argument, NULL, 'd'},
        {"initial-size", required_argument, NULL, 'i'},
        {"range", required_argument, NULL, 'r'},
        {"update-rate", required_argument, NULL, 'u'},
        {"move-rate", required_argument, NULL, 'm'},
        {"snapshot-rate", required_argument, NULL, 'a'},
        {"elasticity", required_argument, NULL, 'x'},
        {NULL, 0, NULL, 0}
    };

    ht_intset_t *set;
    int i, c, size;
    val_t last = 0;
    val_t val = 0;
    thread_data_t *data;
    double duration = DEFAULT_DURATION;
    int initial = DEFAULT_INITIAL;

#if defined(DSL) && defined(STM) && !defined(SEQUENTIAL)
    int nb_app_cores = (RCCE_num_ues() / 2) + ((RCCE_num_ues() % 2) ? 1 : 0);
#else
    int nb_app_cores = RCCE_num_ues();
#endif
    long range = DEFAULT_RANGE;
    int update = DEFAULT_UPDATE;
    int load_factor = DEFAULT_LOAD;
    int move = DEFAULT_MOVE;
    int snapshot = DEFAULT_SNAPSHOT;
    int unit_tx = DEFAULT_ELASTICITY;
    int alternate = DEFAULT_ALTERNATE;
    int effective = DEFAULT_EFFECTIVE;
    unsigned int seed = 0;

    while (1) {
        i = 0;
        c = getopt_long(argc, argv, "hAf:d:i:n:r:s:u:m:a:l:x:", long_options, &i);

        if (c == -1)
            break;

        if (c == 0 && long_options[i].flag == 0)
            c = long_options[i].val;

        switch (c) {
            case 0:
                // Flag is automatically set 
                break;
            case 'h':
                ONCE
            {
                printf("intset -- STM stress test "
                        "(hash table)\n"
                        "\n"
                        "Usage:\n"
                        "  intset [options...]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help\n"
                        "        Print this message\n"
                        "  -A, --Alternate\n"
                        "        Consecutive insert/remove target the same value\n"
                        "  -f, --effective <int>\n"
                        "        update txs must effectively write (0=trial, 1=effective, default=" XSTR(DEFAULT_EFFECTIVE) ")\n"
                        "  -d, --duration <double>\n"
                        "        Test duration in seconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -i, --initial-size <int>\n"
                        "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
                        "  -n, --num-threads <int>\n"
                        "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
                        "  -r, --range <int>\n"
                        "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
                        "  -u, --update-rate <int>\n"
                        "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
                        "  -m , --move-rate <int>\n"
                        "        Percentage of move transactions (default=" XSTR(DEFAULT_MOVE) ")\n"
                        "  -a , --snapshot-rate <int>\n"
                        "        Percentage of snapshot transactions (default=" XSTR(DEFAULT_SNAPSHOT) ")\n"
                        "  -l , --load-factor <int>\n"
                        "        Ratio of keys over buckets (default=" XSTR(DEFAULT_LOAD) ")\n"
                        "  -x, --elasticity (default=4)\n"
                        "        Use elastic transactions\n"
                        "        0 = non-protected,\n"
                        "        1 = normal transaction,\n"
                        "        2 = read elastic-tx,\n"
                        "        3 = read/add elastic-tx,\n"
                        "        4 = read/add/rem elastic-tx,\n"
                        "        5 = elastic-tx w/ optimized move.\n"
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
            case 'n':
                nb_app_cores = atoi(optarg);
                break;
            case 'r':
                range = atol(optarg);
                break;
            case 'u':
                update = atoi(optarg);
                break;
            case 'm':
                move = atoi(optarg);
                break;
            case 'a':
                snapshot = atoi(optarg);
                break;
            case 'l':
                load_factor = atoi(optarg);
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

    if (seed == 0) {
        srand_core();
        seed = rand_range((ID + 17) * 123);
        srand(seed);
    }
    else
        srand(seed);

    assert(duration >= 0);
    assert(initial >= 0);
    assert(nb_app_cores > 0);
    assert(range > 0 && range >= initial);
    assert(update >= 0 && update <= 100);
    assert(move >= 0 && move <= update);
    assert(snapshot >= 0 && snapshot <= (100 - update));
    assert(initial < MAXHTLENGTH);
    assert(initial >= load_factor);

    ONCE
    {
        printf("Set type     : hash table PGAS\n");
        printf("Duration     : %f\n", duration);
        printf("Initial size : %d\n", initial);
        printf("Nb threads   : %d\n", nb_app_cores);
        printf("Value range  : %ld\n", range);
        printf("Update rate  : %d\n", update);
        printf("Load factor  : %d\n", load_factor);
        printf("Move rate    : %d\n", move);
        printf("Snapshot rate: %d\n", snapshot);
        printf("Alternate    : %d\n", alternate);
        printf("Effective    : %d\n", effective);
        FLUSH;
    }

    if ((data = (thread_data_t *) malloc(sizeof (thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    maxhtlength = (unsigned int) initial / load_factor;


    BARRIER

    ONCE
    {
        srand_core();
        udelay(rand_range(123));
        srand_core();

        PGAS_alloc_init(1);
        set = ht_new();

        i = 0;
        maxhtlength = (int) (initial / load_factor);
        while (i < initial) {
            val = rand_range(range);
            if (ht_add(set, val, 0)) {
                last = val;
                i++;
            }
        }
        size = ht_size(set);
        printf("Set size     : %d\n", size);
        printf("Bucket amount: %d\n", maxhtlength);
        printf("Load         : %d\n", load_factor);
        FLUSH

    }
    OTHERS
    {
        if ((set = (ht_intset_t *) malloc(sizeof (ht_intset_t))) == NULL) {
            perror("malloc");
            exit(1);
        }
        if ((set->buckets = (void *) malloc((maxhtlength + 1) * sizeof (intset_t *))) == NULL) {
            perror("malloc");
            exit(1);
        }
        int i;
        for (i = 1; i <= maxhtlength; i++) {
            set->buckets[i]->head = 2*i;
        }
    }

    BARRIER

    PGAS_alloc_init(0);
    PGAS_alloc_offs(initial + (2 * maxhtlength) + 1);

    data->first = last;
    data->range = range;
    data->update = update;
    data->load_factor = load_factor;
    data->move = move;
    data->snapshot = snapshot;
    data->unit_tx = unit_tx;
    data->alternate = alternate;
    data->effective = effective;
    data->nb_add = 0;
    data->nb_added = 0;
    data->nb_remove = 0;
    data->nb_removed = 0;
    data->nb_move = 0;
    data->nb_moved = 0;
    data->nb_snapshot = 0;
    data->nb_snapshoted = 0;
    data->nb_contains = 0;
    data->nb_found = 0;
    data->set = set;
    data->seed = seed;

    BARRIER

    ht_add(set, 10 * ID, 1);
    ht_add(set, 100 * ID, 1);

    BARRIER
    ONCE
    {
        print_ht(set);
    }

    //test(data, duration);

    BARRIER

    printf("---------------------------Thread %d\n", RCCE_ue());
    printf("  #add        : %lu\n", data->nb_add);
    printf("    #added    : %lu\n", data->nb_added);
    printf("  #remove     : %lu\n", data->nb_remove);
    printf("    #removed  : %lu\n", data->nb_removed);
    printf("  #contains   : %lu\n", data->nb_contains);
    printf("    #found    : %lu\n", data->nb_found);
    printf("  #move       : %lu\n", data->nb_move);
    printf("  #moved      : %lu\n", data->nb_moved);
    printf("  #snapshot   : %lu\n", data->nb_snapshot);
    printf("  #snapshoted : %lu\n", data->nb_snapshoted);
#ifdef SEQUENTIAL
    int total_ops = data->nb_add + data->nb_contains + data->nb_remove + data->nb_move + data->nb_snapshot;
    printf("#Ops          : %d\n", total_ops);
    /*
        printf("#Ops/s        : %d\n", (int) (total_ops / duration__));
        printf("#Latency      : %f\n", duration__ / total_ops);
     */
    printf("))) %d\t100\t%.3f\t(Throughput, Commit Rate, Latency)", (int) (total_ops / duration__), 1000 * duration__ / total_ops);
#endif
    FLUSH;

    // Delete set 
    /*
        ht_delete(set);
     */

    free(data);

    BARRIER

    TM_END
    EXIT(0);
}
