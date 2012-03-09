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
#ifdef PLATFORM_CLUSTER
		// we need IDs on networked systems
		nodeid_t nodeId;
#endif

        union {

            struct {

                union {
                    int response; //BOOLEAN
                    unsigned int target; //nodeId
                };

                union {
                    tm_intern_addr_t address; /* address of the data, internal
                    							 representation */
                    int32_t   value;
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

    //TODO: remove ? have them at .c file
    extern BOOLEAN tm_has_command;
    extern int read_value;
    extern nodeid_t* dsl_nodes;


    //void ps_init(void);
    void ps_init_(void);

    /*

     TODO: move the inline function implementations here..

     */

    /* Try to subscribe the TX for reading the address
     */
    CONFLICT_TYPE ps_subscribe(tm_addr_t address);

    /* Try to publish a write on the address
     */
#ifdef PGAS
    CONFLICT_TYPE ps_publish(tm_addr_t address, int value);

    /*  Try to increment by increment and store (so, write) address
     */
    CONFLICT_TYPE ps_store_inc(tm_addr_t address, int increment);
#else
    CONFLICT_TYPE ps_publish(tm_addr_t address);
#endif
    /* Unsubscribes the TX from the address
     */
    void ps_unsubscribe(tm_addr_t address);
    /* Notifies the pub-sub that the publishing is done, so the value
     * is written on the shared mem
     */
    void ps_publish_finish(tm_addr_t address);

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

