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

  //A command to the pub-sub
  typedef struct ps_command_struct 
  {
    int32_t type; //PS_COMMAND_TYPE
    nodeid_t  nodeId;	/* we need IDs on networked systems */
    /* 8 */
    tm_intern_addr_t address; /* addr of the data, internal representation */
    /* 8 */
    union 
    {
      int64_t response; /* used in PS_REMOVE_NODE with PGAS to say persist or not */
      int64_t write_value;
      uint64_t num_words;
    };
    /* 8 */
    uint64_t tx_metadata;

#if defined(PLATFORM_MCORE_SSMP)
    uint8_t padding[32];
#endif
  } PS_COMMAND;

  //A command to the pub-sub
  typedef struct ps_stats_cmd_struct 
  {
    int32_t type; //PS_COMMAND_TYPE
    nodeid_t  nodeId;	/* we need IDs on networked systems */
    /* 8 */

    /* stats collecting --------------------*/
    struct
    {
      union 
      {			
	struct 			/* 12 */
	{
	  uint32_t aborts;
	  uint32_t commits;
	  uint32_t max_retries;
	};
	struct /* 12 */
	{
	  uint32_t aborts_war;
	  uint32_t aborts_raw;
	  uint32_t aborts_waw;
	};
      };
      uint32_t dummy;
      double tx_duration;
    };

#if defined(PLATFORM_MCORE_SSMP)
    uint8_t padding[32];
#endif
  } PS_STATS_CMD_T;


  typedef struct ps_reply_struct 
  {
    int32_t type; //PS_REPLY_TYPE
    nodeid_t nodeId;
    tm_intern_addr_t address; /* address of the data, internal representation */
    int32_t response; //BOOLEAN
    int32_t resp_size;
    int64_t value;

#if defined(PLATFORM_MCORE_SSMP)
    uint8_t padding[32];
#endif
  } PS_REPLY;

#ifdef	__cplusplus
}
#endif

#endif /* _MESSAGING_H_ */
