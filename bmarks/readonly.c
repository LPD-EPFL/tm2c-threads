/* 
 * File:   readonly.c
 * Author: trigonak
 *
 * Created on June 28, 2011, 6:19 PM
 */

#include "../include/tm.h"
#include <getopt.h>
#include "microbench/linkedlist/linkedlist.h"

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

void run_seq(int *);
void run_rand(int *);
void run_uniq(int *);
void run_check_granularity();

#define DEFAULT_DURATION        10
#define DEFAULT_READS           1000
#define DEFAULT_MEM_SIZE        16*1024*1024
#define DEFAULT_SEQUENTIAL      1
#define READ_DURATION


double duration = DEFAULT_DURATION;
int reads = DEFAULT_READS;
int memsize = DEFAULT_MEM_SIZE;
int sequential = DEFAULT_SEQUENTIAL;
#ifdef READ_DURATION
double duration_reads = 0;
#endif

MAIN(int argc, char** argv) {
    TM_INIT

            struct option long_options[] = {
        // These options don't set a flag
        {"help", no_argument, NULL, 'h'},
        {"duration", required_argument, NULL, 'd'},
        {"reads", required_argument, NULL, 'r'},
        {"mem-size", required_argument, NULL, 'm'},
        {"mode", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0}
    };

    int i;
    int c;
    while (1) {
        i = 0;
        c = getopt_long(argc, argv, "ha:d:r:m:s", long_options, &i);

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
                printf("readonly -- Read-only TXs benchmarking\n"
                        "\n"
                        "Usage:\n"
                        "  ro [options...]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help\n"
                        "        Print this message\n"
                        "  -d, --duration <double>\n"
                        "        Test duration in seconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -r, --reads <int>\n"
                        "        Number of reads per transaction (default=" XSTR(DEFAULT_READS) ")\n"
                        "  -m, --mem-size <int>\n"
                        "        Size of memory accessed (default=" XSTR(DEFAULT_MEM_SIZE) ")\n"
                        "  -s, --mode<int>\n"
                        "        Accessing mem sequentially (0), randomly (1), or unique accesses (2) (default=" XSTR(DEFAULT_SEQUENTIAL) ")\n"
                        );
                FLUSH;
            }
                exit(0);
            case 'd':
                duration = atof(optarg);
                break;
            case 'r':
                reads = atoi(optarg);
                break;
            case 'm':
                memsize = atoi(optarg);
                break;
            case 's':
                sequential = atoi(optarg);
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

    srand_core();
    udelay(rand_range(11 * (ID + 1)));
    srand_core();

    int *memory = (int *) RCCE_shmalloc(memsize * sizeof (int));
    if (memory == NULL) {
        PRINT("RCCE_shmalloc memory @ main");
    }

    ONCE
    {
        printf("readonly --\n");
        printf("Duration  \t: %f s\n", duration);
        printf("Reads     \t: %d\n", reads);
        printf("Mem size  \t: %d ints\n", memsize);
        printf("Mode      \t: %d s\n", sequential);
        FLUSH
    }

    BARRIER

/*
    ONCE
    {
*/
        switch (sequential) {
            case 0:
                run_seq(memory);
                break;
            case 1:
                run_rand(memory);
                break;
            case 3:
                run_check_granularity();
                break;
            default:
                run_uniq(memory);
        }
    //}

    BARRIER

    ONCE
    {
#ifdef READ_DURATION
        PRINT("usec/read: %f", duration_reads / (stm_tx_node->tx_commited * reads));
#endif
    }

    BARRIER
    TM_END
    EXIT(0);
}

void run_seq(int* memory) {

    int i;
#ifdef READ_DURATION
    double read_ts_start;
#endif

    FOR(duration) {
        TX_START

#ifdef READ_DURATION
                double read_ts_start = RCCE_wtime();
#endif

        for (i = 0; i < reads; i++) {
            TX_LOAD(memory + (i % memsize));
        }

#ifdef READ_DURATION
        duration_reads += RCCE_wtime() - read_ts_start;
#endif

        TX_COMMIT
    }
}

void run_rand(int* memory) {

    int i;
#ifdef READ_DURATION
    double read_ts_start;
#endif

    FOR(duration) {
        TX_START

#ifdef READ_DURATION
                double read_ts_start = RCCE_wtime();
#endif

        for (i = 0; i < reads; i++) {
            TX_LOAD(memory + rand_range(memsize));
        }

#ifdef READ_DURATION
        duration_reads += RCCE_wtime() - read_ts_start;
#endif

        TX_COMMIT
    }
}

void run_uniq(int* memory) {

    int i;
#ifdef READ_DURATION
    double read_ts_start;
#endif

    FOR(duration) {
        TX_START

#ifdef READ_DURATION
                double read_ts_start = RCCE_wtime();
#endif

        int read = ID;
        for (i = 0; i < reads; i++) {

            TX_LOAD(memory + (read % memsize));
            read += ID;
        }

#ifdef READ_DURATION
        duration_reads += RCCE_wtime() - read_ts_start;
#endif

        TX_COMMIT
    }
}

void run_check_granularity() {
    char *cp = (char *) RCCE_shmalloc(100);

    int i = 10;
    while (i--) {
        TX_START
        if (RCCE_ue() == 1) {
            *cp = 'c';
            TX_LOAD(cp);
            TX_STORE(cp, cp, TYPE_CHAR);
        }
        else if (RCCE_ue() == 3) {
            cp[1] = 'd';
            TX_LOAD(cp + 1);
            TX_STORE(cp + 1, cp + 1, TYPE_CHAR);
        }
        TX_COMMIT
    }
}
