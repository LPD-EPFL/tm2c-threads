/* 
 * File:   readonly.c
 * Author: trigonak
 *
 * Created on June 28, 2011, 6:19 PM
 */

#include "../../include/tm.h"
#include <getopt.h>
#include <stdio.h>

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

/*
 * Returns the offset of a latin char from a (or A for caps) and the int 26 for
 * non letters. Example: input: b, output: 1
 */
inline int char_offset(char c) {
    int a = c - 'a';
    int A = c - 'A';
    if (a < 26 && a >= 0) {
        return a;
    }
    else if (A < 26 && A >= 0) {
        return A;
    }
    else {
        return 26;
    }
}

void map_reduce(FILE *fp, int *chunk_index, int stats);


#define XSTR(s)                 STR(s)
#define STR(s)                  #s

#define DEFAULT_CHUNK_SIZE      1024

int chunk_size = DEFAULT_CHUNK_SIZE;
int stats_local[27] = {};

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
                break;
            case 'r':
                break;
            case 'm':
                break;
            case 's':
                //sequential = atoi(optarg);
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

    FILE *fp;
    fp = fopen("testname", "r");
    if (fp == NULL) {
        PRINT("Could not open file %s\n", "testname");
        EXIT(1);
    }

    PRINT("Opened file %s\n", "testname");

    int *chunk_index = (int *) RCCE_shmalloc(sizeof (int) * 27);
    int *stats = chunk_index + 1;
    if (chunk_index == NULL) {
        PRINT("RCCE_shmalloc memory @ main");
    }

    ONCE
    {
        int i;
        for (i = 0; i < 27; i++) {
            chunk_index[i] = 0;
        }
    }

    ONCE
    {
        fseek(fp, 0L, SEEK_END);
        printf("MapReduce --\n");
        printf("Filesize  \t: %d bytes\n", ftell(fp));
        FLUSH
    }

    BARRIER



    fclose(fp);
    BARRIER
    TM_END
    EXIT(0);
}



#define TX(code)        TX_START code TX_COMMIT

/*
 */
void map_reduce(FILE *fp, int *chunk_index, int stats) {

    int ci;

    TX_START
    ci = *(int *) TX_LOAD(chunk_index);
    int ci1 = ci + 1;
    TX_STORE(chunk_index, &ci1, TYPE_INT);
    //TX_LOAD_STORE(chunk_index, +, 1, TYPE_INT);
    TX_COMMIT

    PRINT("Handling chuck %d", ci);

    char c;
    while (!fseek(fp, ci * chunk_size, SEEK_SET) && c != EOF) {
        int index = 0;
        while (index++ < chunk_size && (c = fgetc(fp)) != EOF) {
            stats_local[char_offset(c)]++;
        }

        TX_START
        ci = *(int *) TX_LOAD(chunk_index);
        int ci1 = ci + 1;
        TX_STORE(chunk_index, &ci1, TYPE_INT);
        //        TX_LOAD_STORE(chunk_index, +, 1, TYPE_INT);
        TX_COMMIT
        
        PRINT("Handling chuck %d", ci);
    }

    PRINT("Updating the statistics");
    char new_local[27] = {};
    TX_START
            int i;
    for (i = 0; i < 27; i++) {
        new_local[i] = (*(int *) TX_LOAD(stats + i)) + stats_local[i];
        TX_STORE(stats + i, &new_local[i], TYPE_INT);
    }
    TX_COMMIT
}