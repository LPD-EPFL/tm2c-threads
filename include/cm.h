/*
  contention management
  author: Trigonakis Vasileios
  date: June 11, 2012
 */
#ifndef CM_H
#define CM_H

#include "common.h"

typedef struct {
    union {
      uint64_t num_txs;
      ticks timestamp;
      double duration;
    };
} cm_metadata_t;
extern cm_metadata_t *cm_metadata_core;

static int32_t * cm_init();

EXINLINED BOOLEAN 
contention_manager(nodeid_t attacker, unsigned short *defenders, CONFLICT_TYPE conflict);

EXINLINED BOOLEAN 
contention_manager_raw_waw(nodeid_t attacker, unsigned short defender, CONFLICT_TYPE conflict);

EXINLINED BOOLEAN 
contention_manager_war(nodeid_t attacker, uint8_t *defenders, CONFLICT_TYPE conflict);

#endif /* CM_H */
