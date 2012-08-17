#ifndef SSHT_LOG_H
#define	SSHT_LOG_H

#ifdef	__cplusplus
extern "C" {
#endif

#define SSHT_LOG_SET_SIZE 1024

  typedef uintptr_t entry_addr_t;
  typedef struct ssht_log_entry {
    entry_addr_t address;    //the address that is locked
    uint32_t index; //the bucket
  } ssht_log_entry_t;

  typedef struct ssht_log_set {
    ssht_log_entry_t *log_entries;
    uint32_t nb_entries;
    uint32_t size;
  } ssht_log_set_t;

  INLINED ssht_log_set_t * ssht_log_set_new() {
    ssht_log_set_t *ssht_log_set;

    if ((ssht_log_set = (ssht_log_set_t *) malloc(sizeof (ssht_log_set_t))) == NULL) {
      perror("Could not initialize the ssht_log set");
      return NULL;
    }

    if ((ssht_log_set->log_entries = (ssht_log_entry_t *) malloc(SSHT_LOG_SET_SIZE * sizeof (ssht_log_entry_t))) == NULL) {
      free(ssht_log_set);
      perror("Could not initialize the ssht_log set");
      return NULL;
    }

    ssht_log_set->nb_entries = 0;
    ssht_log_set->size = SSHT_LOG_SET_SIZE;

    return ssht_log_set;
  }


  INLINED void ssht_log_set_free(ssht_log_set_t *ssht_log_set) {
    free(ssht_log_set->log_entries);
    free(ssht_log_set);
  }


  INLINED ssht_log_set_t * ssht_log_set_empty(ssht_log_set_t *ssht_log_set) {

    ssht_log_set->nb_entries = 0;
    return ssht_log_set;
  }

  INLINED ssht_log_entry_t * ssht_log_set_entry(ssht_log_set_t *ssht_log_set) {
    if (ssht_log_set->nb_entries == ssht_log_set->size) {
      printf("LOCK set max sized (%d)\n", ssht_log_set->size);
      unsigned int new_size = 2 * ssht_log_set->size;
      ssht_log_entry_t *temp;
      if ((temp = (ssht_log_entry_t *) realloc(ssht_log_set->log_entries, new_size * sizeof (ssht_log_entry_t))) == NULL) {
	ssht_log_set_free(ssht_log_set);
	printf("Could not resize the ssht_log set\n");
	return NULL;
      }

      ssht_log_set->log_entries = temp;
      ssht_log_set->size = new_size;
    }

    return &ssht_log_set->log_entries[ssht_log_set->nb_entries++];
  }

  INLINED void ssht_log_set_insert(ssht_log_set_t *ssht_log_set, entry_addr_t address, uint32_t index) {
    ssht_log_entry_t *we = ssht_log_set_entry(ssht_log_set);
    we->address = address;
    we->index = index;
    if (index > 47) {
      printf("index = %d\n", index);
    }
  }

  INLINED ssht_log_entry_t * ssht_log_set_contains(ssht_log_set_t *ssht_log_set, entry_addr_t address, unsigned short index) {
    unsigned int i;
    for (i = ssht_log_set->nb_entries; i-- > 0; ) {
      if ((ssht_log_set->log_entries[i].address == address) && (ssht_log_set->log_entries[i].index==index)) {
	return &ssht_log_set->log_entries[i];
      }
    }

    return NULL;
  }


#ifdef	__cplusplus
}
#endif

#endif	/* LOG_H */

