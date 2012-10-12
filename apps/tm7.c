/*
 * 
 */

#include "tm.h"

/* DEFINES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#define SHMEM_SIZE1      SIS_SIZE
#define NUM_TXOPS       100
#define UPDTX_PRCNT     0
#define WRITE_PRCNT     0
#define DURATION        1

#define ROLL(prcntg)    if (rand_range(100) <= (prcntg))
#define LOST            else


static inline void update_tx(int * sis);
static inline void ro_tx(int * sis);


/* GLOBALS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

unsigned int SIS_SIZE = 200;
unsigned int store_me, ID;
int sum = 0;

MAIN(int argc, char **argv) {

    TM_INIT

    srand_core();
    store_me = ID;
    //SIS_SIZE = NUM_UES * 1000;

    if (argc > 1) {
        SIS_SIZE = atoi(argv[1]);
    }

    int *sis = (int *) sys_shmalloc(SIS_SIZE * sizeof (int));
    if (sis == NULL) {
        PRINT("Error: sys_shmalloc failed");
        EXIT(-1);
    }

    int i;
    for (i = ID; i < SIS_SIZE; i += NUM_UES) {
		NONTX_STORE(&sis[i], -1, TYPE_INT);
    }

    BARRIER

    int txupdate = 0;
    int txro = 0;

    FOR(DURATION) { //seconds

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
            stm_tx_node->tx_commited,
            (int) (stm_tx_node->tx_commited / duration__),
            1000 * (duration__ / stm_tx_node->tx_commited));

    BARRIER

    PF_PRINT

    sys_shfree((sys_t_vcharp) sis);

    TM_END

    fprintf(stderr, "%d", sum);

    EXIT(0);
}

/*
 * Operations executed for a read-only Tx
 */
static inline void ro_tx(int * sis) {
    int i;
    for (i = 0; i < NUM_TXOPS; i++) {
        long rnd = rand_range(SHMEM_SIZE1);
#ifdef PGAS
        sum = TX_LOAD_T(sis + rnd, int);
#else
        int *j = (int *) TX_LOAD(sis + rnd);

        PF_START(0)
        sum = *j;
        PF_STOP(0)
#endif
    }
}

/*
 * Operations executed for an update Tx
 */
static inline void update_tx(int * sis) {
    int i;
    for (i = 0; i < NUM_TXOPS; i++) {
        long rnd = rand_range(SHMEM_SIZE1);

        ROLL(WRITE_PRCNT) {
#ifdef PGAS
            TX_STORE(sis + rnd, ID, TYPE_INT);
#else
            TX_STORE(sis + rnd, &ID, TYPE_INT);
#endif
        }
        LOST
        {
            ro_tx(sis);
        }
    }
}
