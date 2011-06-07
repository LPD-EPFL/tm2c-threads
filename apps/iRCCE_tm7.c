/*
 * 
 */

#include "tm.h"

/* DEFINES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#define SHMEM_SIZE      SIS_SIZE
#define NUM_TXOPS       100
#define UPDTX_PRCNT     20
#define WRITE_PRCNT     20

#define FOR(seconds)    double starting__ = RCCE_wtime(), duration__; while ((duration__ = (RCCE_wtime() - starting__)) < (seconds))
#define ROLL(prcntg)    if (rand_range(100) <= (prcntg))
#define LOST            else


/* FUNCTIONS / FUNCTION HEADERS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

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

inline void update_tx(int * sis);
inline void ro_tx(int * sis);


/* GLOBALS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

unsigned int SIS_SIZE = 4800;
unsigned int store_me, ID;

MAIN(int argc, char **argv) {

    TM_INIT

    srand_core();
    store_me = ID;
    SIS_SIZE = NUM_UES * 1000;

    if (argc > 1) {
        SIS_SIZE = atoi(argv[1]);
    }

    int *sis = (int *) RCCE_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINT("RCCE_shmalloc");
        EXIT(-1);
    }

    int i;
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
        sis[i] = -1;
    }

    BARRIER
    
            int txupdate = 0;
    int txro = 0;

    FOR(1) { //seconds

        TX_START

        ROLL(UPDTX_PRCNT) {
            //update tx
            txupdate++;

            update_tx(sis);
        }
        LOST
        {
            txro++;
            //read-only tx

            ro_tx(sis);
        }

        TX_COMMIT
    }
    
    PRINT("%02d\t%d\t%d\t%f", NUM_UES,
            stm_tx_node->tx_commited, (int) (stm_tx_node->tx_commited / duration__),  1000*(duration__ / stm_tx_node->tx_commited));

    BARRIER

    RCCE_shfree((t_vcharp) sis);

    TM_END

    EXITALL(0);
}

/*
 * Operations executed for an update Tx
 */
inline void update_tx(int * sis) {
    int i;
    for (i = 0; i < NUM_TXOPS; i++) {
        long rnd = rand_range(SHMEM_SIZE);

        ROLL(WRITE_PRCNT) {
            TX_STORE(sis + rnd, &ID, TYPE_INT);
        }
        LOST
        {
            int j = *(int *) TX_LOAD(sis + rnd);
        }
    }
}

/*
 * Operations executed for a read-only Tx
 */
inline void ro_tx(int * sis) {
    int i;
    for (i = 0; i < NUM_TXOPS; i++) {
        long rnd = rand_range(SHMEM_SIZE);
        int j = *(int *) TX_LOAD(sis + rnd);
    }
}