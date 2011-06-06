#include "pubSubTM.h"
#include "hashtable.h"

#define I 1000
#define J 48
#define ADDR 480

void run(void * args);

MAIN(int argc, char **argv) {
    RCCE_init(&argc, &argv);

    taskcreate(run, NULL, 20480);

    int i = 2000;
    while (i-- > 100) {
        taskudelay(12);
    }
    PRINTD("main: done . . . 1");

    while (i-- > 0) {
        taskudelay(12);
    }
    PRINTD("main: done . . .");

    taskexit(0);
}

void run(void * args) {
    ps_hashtable_t ps_hashtable = ps_hashtable_new();

    int i, j;
    double start = RCCE_wtime();
    for (i = 0; i < I; i++) {
        for (j = 0; j < J; j++) {
            
            int ij = i*j;
            int addr = ij % ADDR;
            ps_hashtable_insert(ps_hashtable, j, addr, READ);

            if (ij % 13 == 0) {
                ps_hashtable_insert(ps_hashtable, j + 1, addr + 1, WRITE);
            }

        }
    }

    double diff = RCCE_wtime() - start;
    PRINTD("insterted %d entries in %f micros | avg: %f", i*j, diff, diff / (i * j));

    /*
        ps_hashtable_print(ps_hashtable);
     */

    start = RCCE_wtime();
    for (i = 0; i < I; i++) {
        for (j = 0; j < J; j++) {
            int ij = i*j;
            int addr = ij % ADDR;
            ps_hashtable_delete(ps_hashtable, j, addr, READ);

            if (ij % 13 == 0)
                ps_hashtable_delete(ps_hashtable, j + 1, addr + 1, WRITE);
        }
    }

    diff = RCCE_wtime() - start;
    PRINTD("deleted %d entries in %f micros | avg: %f", i*j, diff, diff / (i * j));

    // ps_hashtable_print(ps_hashtable);

    RCCE_finalize();
    taskexit(0);
}