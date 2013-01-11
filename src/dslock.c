/* 
 * File:   pubSubTM.c
 * Author: trigonak
 *
 * Created on March 7, 2011, 4:45 PM
 * working
 */

#include "dslock.h"
#include <stdio.h> //TODO: clean the includes
#include <unistd.h>
#include <math.h>

ps_hashtable_t ps_hashtable;

#ifdef PGAS
write_set_pgas_t **PGAS_write_sets;
#endif

unsigned long int stats_total = 0,
                  stats_commits = 0,
                  stats_aborts = 0,
                  stats_max_retries = 0,
                  stats_aborts_war = 0,
                  stats_aborts_raw = 0,
                  stats_aborts_waw = 0,
                  stats_received = 0;
double stats_duration = 0;

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
cm_metadata_t *cm_metadata_core;
#endif

#ifdef DEBUG_UTILIZATION
extern unsigned int read_reqs_num;
extern unsigned int write_reqs_num;
int bucket_usages[NUM_OF_BUCKETS];
int bucket_current[NUM_OF_BUCKETS];
int bucket_max[NUM_OF_BUCKETS];
#endif

extern void tm_term();

void dsl_init(void) {
#ifdef PGAS
    PGAS_write_sets = (write_set_pgas_t **) malloc(NUM_UES * sizeof (write_set_pgas_t *));
    if (PGAS_write_sets == NULL) {
        PRINT("malloc PGAS_write_sets == NULL");
        EXIT(-1);
    }

    nodeid_t j;
    for (j = 0; j < NUM_UES; j++) {
        if (is_app_core(j)) { /*only for non DSL cores*/
            PGAS_write_sets[j] = write_set_pgas_new();
            if (PGAS_write_sets[j] == NULL) {
                PRINT("malloc PGAS_write_sets[i] == NULL");
                EXIT(-1);
            }
        }
    }

#endif

    ps_hashtable = ps_hashtable_new();

#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
    cm_metadata_core = (cm_metadata_t *) calloc(NUM_UES, sizeof(cm_metadata_t));
    if (cm_metadata_core == NULL) {
        PRINT("calloc @ dsl_init");
    }
#endif

    sys_dsl_init();

    /* PRINT("[DSL NODE] Initialized pub-sub.."); */
    dsl_communication();

    BARRIERW;
    tm_term();
    
    term_system();
    EXIT(0);
}

void print_global_stats() {
    stats_duration /= NUM_APP_NODES;

    printf("||||||||||||||||||||||||||||||||||||||||||||||||||||||\n");
    printf("TXs Statistics : %02d Nodes, %02d App Nodes|||||||||||||||||||||||\n", NUM_UES, NUM_APP_NODES);
    printf(":: TOTAL -----------------------------------------------------\n");
    printf("T | Avg Duration\t: %.3f s\n", stats_duration);
    printf("T | Starts      \t: %lu\n", stats_total);
    printf("T | Commits     \t: %lu\n", stats_commits);
    printf("T | Aborts      \t: %lu\n", stats_aborts);
    printf("T | Max Retries \t: %lu\n", stats_max_retries);
    printf("T | Aborts WAR  \t: %lu\n", stats_aborts_war);
    printf("T | Aborts RAW  \t: %lu\n", stats_aborts_raw);
    printf("T | Aborts WAW  \t: %lu\n", stats_aborts_waw);

    stats_aborts /= stats_duration;
    stats_aborts_raw /= stats_duration;
    stats_aborts_war /= stats_duration;
    stats_aborts_waw /= stats_duration;
    stats_commits /= stats_duration;
    stats_total /= stats_duration;

    unsigned long int stats_commits_total = stats_commits;

    printf(":: PER SECOND TOTAL AVG --------------------------------------\n");
    printf("TA| Starts      \t: %lu\t/s\n", stats_total);
    printf("TA| Commits     \t: %lu\t/s\n", stats_commits);
    printf("TA| Aborts      \t: %lu\t/s\n", stats_aborts);
    printf("TA| Aborts WAR  \t: %lu\t/s\n", stats_aborts_war);
    printf("TA| Aborts RAW  \t: %lu\t/s\n", stats_aborts_raw);
    printf("TA| Aborts WAW  \t: %lu\t/s\n", stats_aborts_waw);

    int stats_commits_app = stats_commits / NUM_APP_NODES;

    stats_aborts /= NUM_UES;
    stats_aborts_raw /= NUM_UES;
    stats_aborts_war /= NUM_UES;
    stats_aborts_waw /= NUM_UES;
    stats_commits /= NUM_UES;
    stats_total /= NUM_UES;

    double commit_rate = (stats_total - stats_aborts) / (double) stats_total;
    double tx_latency = (1 / (double) stats_commits_app) * 1000 * 1000; //micros

    printf(":: PER SECOND PER NODE AVG -----------------------------------\n");
    printf("NA| Starts      \t: %lu\t/s\n", stats_total);
    printf("NA| Commits     \t: %lu\t/s\n", stats_commits);
    printf("NA| Aborts      \t: %lu\t/s\n", stats_aborts);
    printf("NA| Aborts WAR  \t: %lu\t/s\n", stats_aborts_war);
    printf("NA| Aborts RAW  \t: %lu\t/s\n", stats_aborts_raw);
    printf("NA| Aborts WAW  \t: %lu\t/s\n", stats_aborts_waw);
    printf(":: Collect data ----------------------------------------------\n");
    printf("))) %-10lu%-7.2f%-.3f%5d\t(Throughput, Commit Rate, Latency)\n", stats_commits_total, commit_rate * 100, tx_latency, NUM_DSL_NODES);
    printf("||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n");

    fflush(stdout);
}

