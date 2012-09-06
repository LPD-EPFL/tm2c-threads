#ifndef _MESSAGING_H_
#define _MESSAGING_H_

/*
 * In this header file, we defined the messages that nodes use to communicate.
 * This way, we can include just the data definition in necessary files.
 */

#ifdef	__cplusplus
extern "C" {
#endif

  typedef enum {
    PS_SUBSCRIBE, //0
    PS_PUBLISH, //1
    PS_UNSUBSCRIBE, //2
    PS_PUBLISH_FINISH, //3
    PS_REMOVE_NODE, //7
    PS_LOAD_NONTX, //8
    PS_STORE_NONTX, //9
    PS_WRITE_INC,
    PS_STATS,
    PS_UKNOWN
  } PS_COMMAND_TYPE;

  typedef enum {
    PS_SUBSCRIBE_RESPONSE,
    PS_PUBLISH_RESPONSE,
    PS_ABORTED,
    PS_LOAD_NONTX_RESPONSE,
    PS_STORE_NONTX_RESPONSE,
    PS_UKNOWN_RESPONSE
  } PS_REPLY_TYPE;

  //TODO: make it union with address normal int..
  //A command to the pub-sub
typedef struct ps_command_struct {
        unsigned int type; //PS_COMMAND_TYPE
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_MCORE) || defined(PLATFORM_TILERA)
        // we need IDs on networked systems
        nodeid_t nodeId;
#endif

        union {

            struct {

                union {
                    int response; /* used in PS_REMOVE_NODE with PGAS to say persist or not */
                    int write_value;
                };

                tm_intern_addr_t address; /* address of the data, internal
				       representation */
            };

            //stats collecting

            union {

                struct {
                    unsigned int commits;
                    unsigned int aborts;
                    unsigned int max_retries;
                };

                struct {
                    unsigned int aborts_war;
                    unsigned int aborts_raw;
                    unsigned int aborts_waw;

                };
            };
        };

        union {
            double tx_duration;
            uint64_t tx_metadata;
            double stats_msg_seq; //not 0 in the first status msg, 0 in the second status msg
        };
        int dummy_to_make_it_32_bytes;
    } PS_COMMAND;

  typedef struct {
    unsigned int type; //PS_REPLY_TYPE
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_MCORE)
    // we need IDs on networked systems
    nodeid_t nodeId;
#endif

    union {

      struct {

	int response; //BOOLEAN

	union {
	  tm_intern_addr_t address; /* address of the data, internal
				       representation */
	  int32_t value;
	};
      };
    };

  } PS_REPLY;

#ifdef	__cplusplus
}
#endif

#endif /* _MESSAGING_H_ */
