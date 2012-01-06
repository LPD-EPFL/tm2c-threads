/* 
 * File:   pubSubTM.h
 * Author: trigonak
 *
 * Created on March 7, 2011, 10:50 AM
 * 
 * The main DTM system functionality
 * 
 * Subscribe == read (read-lock)
 * Publish == write (write-lock)
 */

#ifndef PUBSUBTM_H
#define	PUBSUBTM_H

#include "common.h"
#include "stm.h"

#ifdef OLDHASH
#include "hashtable.h"
#else
#include "rwhash.h"
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define DHT_ADDRESS_MASK 3

    //the buffer size for pub-sub = the size (in bytes) of the msg exchanged
#define PS_BUFFER_SIZE     32

    //the number of operators that pub-sub supports
#define PS_NUM_OPS         6
    //pub-sub request types

    typedef enum {
        PS_SUBSCRIBE, //0
        PS_PUBLISH, //1
        PS_UNSUBSCRIBE, //2
        PS_PUBLISH_FINISH, //3
        PS_SUBSCRIBE_RESPONSE, //4
        PS_PUBLISH_RESPONSE, //5
        PS_ABORTED, //6
        PS_REMOVE_NODE, //7
        PS_STATS,
        PS_WRITE_INC
    } PS_COMMAND_TYPE;

    //TODO: make it union with address normal int..
    //A command to the pub-sub

    typedef struct {
        unsigned int type; //PS_COMMAND_TYPE

        union {

            struct {

                union {
                    int response; //BOOLEAN
                    unsigned int target; //nodeId
                };

                union {
                    unsigned int address;
                    int value;
                };
            };

            //stats collecting

            struct {
                unsigned short commits;
                unsigned short aborts;
                unsigned short max_retries;
                unsigned short aborts_war;
                unsigned short aborts_raw;
                unsigned short aborts_waw;
            };
        };

        union {
            double tx_duration;

            struct {
                unsigned int abrt_attacker;
                unsigned int abrt_reason;
            };

            int write_value;
        };
    } PS_COMMAND;


    typedef unsigned int SHMEM_START_ADDRESS;


    //TODO: remove ? have them at .c file
    extern iRCCE_WAIT_LIST waitlist; //the send-recv buffer
    extern BOOLEAN tm_has_command;
    extern PS_COMMAND *ps_command;
    extern int read_value;


    //void ps_init(void);
    void ps_init_(void);
    void dsl_init(void);
    /* Pushes both the sends and receive messages that queued in the node.
     * Pushing is necessary in order to proceed, else there could be a
     * "deadlock" case where no node delivers any messages.
     */
    inline void iRCCE_ipush(void);


    /*

     TODO: move the inline function implementations here..

     */

    /* Try to subscribe the TX for reading the address
     */
#ifdef PGAS
    CONFLICT_TYPE ps_subscribe(unsigned int address);
#else
    CONFLICT_TYPE ps_subscribe(void *address);
#endif
    /* Try to publish a write on the address
     */
#ifdef PGAS
    CONFLICT_TYPE ps_publish(unsigned int address, int value);

    /*  Try to increment by increment and store (so, write) address
     */
    CONFLICT_TYPE ps_store_inc(unsigned int address, int increment);
#else
    CONFLICT_TYPE ps_publish(void *address);
#endif
    /* Unsubscribes the TX from the address
     */
    void ps_unsubscribe(void *address);
    /* Notifies the pub-sub that the publishing is done, so the value
     * is written on the shared mem
     */
    void ps_publish_finish(void *address);

    void ps_finish_all(CONFLICT_TYPE conflict);
    //
    //    unsigned int shmem_address_offset(void *shmem_address);
    //
    //    unsigned int DHT_get_responsible_node(void * shmem_address);

    void ps_send_stats(stm_tx_node_t *, double duration);
#ifdef	__cplusplus
}
#endif

#endif	/* PUBSUBTM_H */

