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

void map_reduce(FILE *fp, int *chunk_index, int *stats);
void map_reduce_seq(FILE *fp, int *chunk_index, int *stats);


#define XSTR(s)                 STR(s)
#define STR(s)                  #s

#define DEFAULT_CHUNK_SIZE      1024
#define DEFAULT_FILENAME        "testname"

int chunk_size = DEFAULT_CHUNK_SIZE;
int stats_local[27] = {};
char *filename = DEFAULT_FILENAME;
int sequential = 0;

MAIN(int argc, char** argv) {
    TM_INIT

            struct option long_options[] = {
        // These options don't set a flag
        {"help", no_argument, NULL, 'h'},
        {"filename", required_argument, NULL, 'f'},
        {"chunk", required_argument, NULL, 'c'},
        {"seq", no_argument, NULL, 's'},
        {NULL, 0, NULL, 0}
    };

    int i;
    int c;
    while (1) {
        i = 0;
        c = getopt_long(argc, argv, "ha:f:c:s", long_options, &i);

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
                printf("MapReduce -- A transactional MapReduce example\n"
                        "\n"
                        "Usage:\n"
                        "  mr [options...]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help\n"
                        "        Print this message\n"
                        "  -f, --filename <string>\n"
                        "        File to be loaded and processed\n"
                        "  -c, --chunk <int>\n"
                        "        Chuck size (default=" XSTR(DEFAULT_CHUNK_SIZE) ")\n"
                        "  -s, --seq <int>\n"
                        "        Sequential execution\n"

                        );
                FLUSH;
            }
                exit(0);
            case 'f':
                filename = optarg;
                break;
            case 'c':
                chunk_size = atoi(optarg);
                break;
            case 's':
                sequential = 1;
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
    char fn[100];
    strcpy(fn, "/shared/trigonak/");
    strcat(fn, filename);
    fp = fopen(fn, "r");
    if (fp == NULL) {
        PRINT("Could not open file %s\n", fn);
        EXIT(1);
    }

    int *chunk_index = (int *) sys_shmalloc(sizeof (int));
    int *stats = (int *) sys_shmalloc(sizeof (int) * 27);
    if (chunk_index == NULL || stats == NULL) {
        PRINT("sys_shmalloc memory @ main");
        EXIT(1);
    }

    ONCE
    {
        PRINT("Opened file %s\n", fn);
        int i;
        for (i = 0; i < 27; i++) {
            stats[i] = 0;
        }
        *chunk_index = 0;

        fseek(fp, 0L, SEEK_END);
        printf("MapReduce --\n");
        printf("Fillename \t: %s\n", filename);
        printf("Filesize  \t: %ld bytes\n", ftell(fp));
        printf("Chunksize \t: %d bytes\n", chunk_size);
        FLUSH
    }

    BARRIER

    if (sequential) {
        map_reduce_seq(fp, chunk_index, stats);
    }
    else {
        map_reduce(fp, chunk_index, stats);
    }

    fclose(fp);
    BARRIER

    ONCE
    {
        printf("TOTAL stats - - - - - - - - - - - - - - - -\n");
        int i;
        for (i = 0; i < 26; i++) {
            printf("%c :\t %d\n", 'a' + i, stats[i]);
        }
        printf("Other :\t %d\n", stats[i]);
    }

    TM_END
    EXIT(0);
}



/*
 */
void map_reduce(FILE *fp, int *chunk_index, int *stats) {

    int ci;

    udelay((50000 * (ID + 1)));

    duration__ = wtime();

    TX_START
    //ci = *(int *) TX_LOAD(chunk_index);
    //int ci1 = ci + 1;
    //TX_STORE(chunk_index, &ci1, TYPE_INT);
    TX_LOAD_STORE(chunk_index, +, 1, TYPE_INT);
    ci = *chunk_index;
    //TX_COMMIT
    TX_COMMIT_NO_PUB


            char c;
    while (!fseek(fp, ci * chunk_size, SEEK_SET) && c != EOF) {
        PRINTD("Handling chuck %d", ci);

        int index = 0;
        while (index++ < chunk_size && (c = fgetc(fp)) != EOF) {
            stats_local[char_offset(c)]++;
        }

        TX_START
        //ci = *(int *) TX_LOAD(chunk_index);
        //int ci1 = ci + 1;
        //TX_STORE(chunk_index, &ci1, TYPE_INT);
        TX_LOAD_STORE(chunk_index, +, 1, TYPE_INT);
        ci = *chunk_index;
        //TX_COMMIT
        TX_COMMIT_NO_PUB

    }

    duration__ = wtime() - duration__;

    PRINTD("Updating the statistics");
    int new_local[27];
    int i;

    TX_START
    for (i = 0; i < 27; i++) {
/*
        int stat = (*(int *) TX_LOAD(stats + i));
        PRINTD("[%c : %d | %d]", 'a' + i, stat, stats[i]);
        new_local[i] = stat + stats_local[i];
        TX_STORE(stats + i, &new_local[i], TYPE_INT);
*/
        TX_LOAD_STORE(stats + i, +, stats_local[i], TYPE_INT);
    }
    //TX_COMMIT
    TX_COMMIT_NO_PUB


    for (i = 0; i < 27; i++) {
        PRINTF("%c : %d\n", 'a' + i, stats_local[i]);
    }
    FLUSH
}

/*
 */
void map_reduce_seq(FILE *fp, int *chunk_index, int *stats) {

        int ci;

        duration__ = wtime();

        rewind(fp);
        char c;

        while ((c = fgetc(fp)) != EOF) {
            stats_local[char_offset(c)]++;
        }

        duration__ = wtime() - duration__;

        PRINTD("Updating the statistics");

        int i;
        for (i = 0; i < 27; i++) {
            stats[i] += stats_local[i];
            PRINTF("%c : %d\n", 'a' + i, stats_local[i]);
        }
        FLUSH

/*
    int ci;

    duration__ = wtime();

    ci = (*chunk_index)++;

    char c;
    while (!fseek(fp, ci * chunk_size, SEEK_SET) && c != EOF) {
        PRINTD("Handling chuck %d", ci);

        int index = 0;
        while (index++ < chunk_size && (c = fgetc(fp)) != EOF) {
            stats_local[char_offset(c)]++;
        }

        ci = (*chunk_index)++;
    }

    duration__ = wtime() - duration__;

    PRINTD("Updating the statistics");

    int i;
    for (i = 0; i < 27; i++) {
        PRINTD("[%c : %d | %d]", 'a' + i, stat, stats[i]);
        stats[i] += stats_local[i];
    }


    for (i = 0; i < 27; i++) {
        PRINTF("%c : %d\n", 'a' + i, stats_local[i]);
    }
    FLUSH
*/

}
