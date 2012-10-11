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
    PS_SUBSCRIBE,               //0
    PS_PUBLISH,	                //1
    PS_UNSUBSCRIBE,		//2
    PS_PUBLISH_FINISH,		//3
    PS_REMOVE_NODE,		//4
    PS_LOAD_NONTX,		//5
    PS_STORE_NONTX,		//6
    PS_WRITE_INC,		//7
    PS_CAS,			//8
    PS_STATS,
    PS_UKNOWN
  } PS_COMMAND_TYPE;

  typedef enum {
    PS_SUBSCRIBE_RESPONSE,
    PS_PUBLISH_RESPONSE,
    PS_ABORTED,
    PS_CAS_RESPONSE,
    PS_LOAD_NONTX_RESPONSE,
    PS_STORE_NONTX_RESPONSE,
    PS_UKNOWN_RESPONSE
  } PS_REPLY_TYPE;

  //TODO: make it union with address normal int..
  //A command to the pub-sub
  typedef struct ALIGNED(64) ps_command_struct {
    unsigned int type; //PS_COMMAND_TYPE
#if defined(PLATFORM_CLUSTER) || defined(PLATFORM_TILERA) || defined(PLATFORM_MCORE_SHRIMP)
    nodeid_t nodeId;	/* we need IDs on networked systems */
#else
    uint32_t oldval;
#endif
    union {
      struct {
	union {
	  int response; /* used in PS_REMOVE_NODE with PGAS to say persist or not */
	  int write_value;
	};
	tm_intern_addr_t address; /* addr of the data, internal representation */
      };
      union {			/* stats collecting */
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
    union { 			/* cm metadata */
      double tx_duration;
      uint64_t tx_metadata;
    };
  } PS_COMMAND;

  typedef ALIGNED(64) struct {
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

#if defined(PLATFORM_MCORE_SSMP)
    uint8_t padding[40];
#endif
  } PS_REPLY;

#ifdef	__cplusplus
}
#endif

#endif /* _MESSAGING_H_ */
