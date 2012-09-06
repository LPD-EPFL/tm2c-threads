/*
  contention management
  author: Trigonakis Vasileios
  date: June 11, 2012
 */

#include "cm.h"

#include "ps_hashtable.h"
extern ps_hashtable_t ps_hashtable;

static int32_t * cm_init();

BOOLEAN 
contention_manager(nodeid_t attacker, unsigned short *defenders, CONFLICT_TYPE conflict) {
     switch (conflict) {
        case READ_AFTER_WRITE:
        case WRITE_AFTER_WRITE:
            if (cm_metadata_core[attacker].timestamp < cm_metadata_core[*defenders].timestamp ||
		(cm_metadata_core[attacker].timestamp == cm_metadata_core[*defenders].timestamp && attacker < *defenders)) {
                //new TX - attacker won
                abort_node((unsigned int) *defenders, conflict);
                    //it was running and the current node aborted it
                PRINTD("[force abort] %d (%d) for %d (%d)",
                        *defenders, cm_metadata_core[*defenders].num_txs,
                        attacker,
                        cm_metadata_core[attacker].num_txs);
                return TRUE;
            } 
            else { //existing TX won
                PRINTD("[norml abort] %d (%d) for %d (%d)",
                        attacker,
                        cm_metadata_core[attacker].num_txs,
                        *defenders, cm_metadata_core[*defenders].num_txs);
                return FALSE;
            }
        case WRITE_AFTER_READ:
        {
            int i;
            for (i = 0; i < NUM_UES; i++) {
                if (defenders[i]) {
                    if (cm_metadata_core[attacker].timestamp > cm_metadata_core[i].timestamp ||
			(cm_metadata_core[attacker].timestamp == cm_metadata_core[i].timestamp && i < attacker)) {
                        PRINTD("[norml abort] %d (%d) for %d (%d)",
                                attacker,
                                cm_metadata_core[attacker].num_txs,
                                i, cm_metadata_core[i].num_txs);
                            
                            return FALSE;
                        }
                    }
                }
                //attacker won all readers
                //TODO: handle the aborting
                for (i = 0; i < NUM_UES; i++) {
                    if (defenders[i]) {
                        abort_node((unsigned int) i, conflict);
                    }
            }

            PRINTD("[force abort] READERS for %d (%d)",
                    attacker,
                    cm_metadata_core[attacker].num_txs);
                return TRUE;
            }
        }
        //to avoid non ret warning
        return FALSE;
}

BOOLEAN 
contention_manager_raw_waw(nodeid_t attacker, unsigned short defender, CONFLICT_TYPE conflict) {
  if (cm_metadata_core[attacker].timestamp < cm_metadata_core[defender].timestamp ||
      (cm_metadata_core[attacker].timestamp == cm_metadata_core[defender].timestamp && attacker < defender)) {
    //new TX - attacker won
    abort_node((unsigned int) defender, conflict);
    ps_hashtable_delete_node(ps_hashtable, defender);
    //it was running and the current node aborted it
    return TRUE;
  } 
  else { //existing TX won
    return FALSE;
  }

  return FALSE;
}

BOOLEAN 
contention_manager_war(nodeid_t attacker, uint8_t *defenders, CONFLICT_TYPE conflict) {
  uint32_t i;
  for (i = 0; i < NUM_UES; i++) {
    if (defenders[i]) {
      if (cm_metadata_core[attacker].timestamp > cm_metadata_core[i].timestamp ||
	  (cm_metadata_core[attacker].timestamp == cm_metadata_core[i].timestamp && i < attacker)) {
	PRINTD("[norml abort] %d (%d) for %d (%d)",
	       attacker,
	       cm_metadata_core[attacker].num_txs,
	       i, cm_metadata_core[i].num_txs);
                            
	return FALSE;
      }
    }
  }
  //attacker won all readers
  //TODO: handle the aborting
  for (i = 0; i < NUM_UES; i++) {
    if (defenders[i]) {
      //      defenders[i] = 0;
      abort_node((unsigned int) i, conflict);
      ps_hashtable_delete_node(ps_hashtable, i);
    }
  }

  return TRUE;
}

void contention_manager_pri_print() {
  uint32_t reps = TOTAL_NODES();
  uint32_t *found = (uint32_t *) calloc(reps, sizeof(uint32_t));
  assert(found != NULL);

  printf("\t");

  while (reps--) {
    uint64_t max = 100000000, max_index = max;
    uint32_t i;
    for (i = 0; i < TOTAL_NODES(); i++) {
      if (!found[i]) {
	if (cm_metadata_core[i].timestamp < max ||
	  (cm_metadata_core[i].timestamp == max && i < max_index)) {
	  max = cm_metadata_core[i].timestamp;
	  max_index = i;
	}
      }
    }
    found[max_index] = 1;
    if (max > 0) {
      printf("%02d [%d] > ", max_index, max);
    }
  }
  printf("none\n");
  FLUSH;
}
