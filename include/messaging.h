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


  /* A command to the DSL service */
#if !defined(PLATFORM_TILERA)
  typedef struct ps_command_struct 
  {
    int32_t type;	      /* PS_COMMAND_TYPE */
    nodeid_t nodeId;	      /* we need IDs on networked systems */
    /* 8 */
    tm_intern_addr_t address; /* addr of the data, internal representation */
#  if defined(SCC)
    int32_t dummy[1];		/* tm_inter_add = size_t = size 4 on the SCC */
#  endif
    /* 8 */
    union 
    {
      int64_t response;       /* used in PS_REMOVE_NODE with PGAS to say persist or not */
      int64_t write_value;
      uint64_t num_words;
    };
    /* 8 */
    uint64_t tx_metadata;

    /* SSMP[.SCC] uses the last word as a flag, hence it should be not used for data */
#  if defined(PLATFORM_MCORE_SSMP)
    uint8_t padding[32];
#  endif
  } PS_COMMAND;


  /* a reply message */
  typedef struct ps_reply_struct 
  {
    int32_t type; //PS_REPLY_TYPE
    nodeid_t nodeId;
    tm_intern_addr_t address; /* address of the data, internal representation */
#  if defined(SCC)
    int32_t dummy[1];		/* tm_inter_add = size_t = size 4 on the SCC */
#  endif
    int64_t value;
    int32_t response; //BOOLEAN
    int32_t resp_size;
    /* SSMP[.SCC] uses the last word as a flag, hence it should be not used for data */
#  if defined(PLATFORM_MCORE_SSMP)
    uint8_t padding[32];
#  endif
  } PS_REPLY;

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

    /* SSMP[.SCC] uses the last word as a flag, hence it should be not used for data */
#if defined(SCC)
    double tx_duration;
    uint32_t mpb_flag;	/* on the SCC, we have CL (MPB) line of 32 bytes */
#else
    uint32_t dummy;
    double tx_duration;
#endif      
  };

#if defined(PLATFORM_MCORE_SSMP)
  uint8_t padding[32];
#endif
} PS_STATS_CMD_T;


  /* ____________________________________________________________________________________________________ */
#else  /* PLATFORM_TILERA                                                                                 */
  /* ____________________________________________________________________________________________________ */

#  if !defined(NOCM) && defined(PGAS)
#    define PS_COMMAND_SIZE       24
#    define PS_COMMAND_SIZE_WORDS 7
#  elif !defined(NOCM) || defined(PGAS)
#    define PS_COMMAND_SIZE       16
#    define PS_COMMAND_SIZE_WORDS 4
#  else
#    define PS_COMMAND_SIZE       8
#    define PS_COMMAND_SIZE_WORDS 2
#  endif

  typedef struct ps_command_struct 
  {
    uint8_t type;	      /* PS_COMMAND_TYPE */
    uint8_t nodeId;	      /* we need IDs on networked systems */
    /* 4*/
    tm_intern_addr_t address; /* addr of the data, internal representation */
    /* 8 */ 

    /* OPTIONAL based on PGAS and NOCM */
#  if defined(PGAS)
    union 
    {
      int64_t response;       /* used in PS_REMOVE_NODE with PGAS to say persist or not */
      int64_t write_value;
      uint64_t num_words;
    };
#  endif	/* PGAS */
#  if !defined(NOCM)
    uint64_t tx_metadata;
#  endif  /* !NOCM */
  } PS_COMMAND;


#  if defined(PGAS)
#    define PS_REPLY_SIZE       16
#    define PS_REPLY_SIZE_WORDS 4
#  else
#    define PS_REPLY_SIZE       8
#    define PS_REPLY_SIZE_WORDS 2
#  endif

  typedef struct ps_reply_struct 
  {
  uint8_t type;
  uint8_t nodeId;
  uint8_t response;
  uint8_t resp_size;
  tm_intern_addr_t address; /* address of the data, internal representation */

#if defined(PGAS)
  int64_t value;
#endif	/* PGAS */
} PS_REPLY;


#define PS_STATS_CMD_SIZE       32
#define PS_STATS_CMD_SIZE_WORDS 8

//A command to the pub-sub
typedef struct ps_stats_cmd_struct 
{
  uint8_t type; //PS_COMMAND_TYPE
  uint8_t  nodeId;	/* we need IDs on networked systems */
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

    /* SSMP[.SCC] uses the last word as a flag, hence it should be not used for data */
#if defined(SCC)
    double tx_duration;
    uint32_t mpb_flag;	/* on the SCC, we have CL (MPB) line of 32 bytes */
#else
    uint32_t dummy;
    double tx_duration;
#endif      
  };

#if defined(PLATFORM_MCORE_SSMP)
  uint8_t padding[32];
#endif
} PS_STATS_CMD_T;

#endif  /* !PLATFORM_TILERA */

#ifdef	__cplusplus
}
#endif

#endif /* _MESSAGING_H_ */
