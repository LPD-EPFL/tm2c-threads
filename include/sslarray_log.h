#ifndef SSLARRAY_LOG_H
#define	SSLARRAY_LOG_H

#include "sslarray.h"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef INLINED
#if __GNUC__ && !__GNUC_STDC_INLINE__
#define INLINED static inline __attribute__((always_inline))
#else
#define INLINED inline
#endif
#endif

#define SSLARRAY_LOG_SET_SIZE 1024

  typedef uint32_t entry_addr_t;
  typedef uint32_t sslarray_log_entry_t;

  typedef struct sslarray_log_set {
    sslarray_log_entry_t *log_entries;
    uint32_t nb_entries;
    uint32_t size;
  } sslarray_log_set_t;

  INLINED sslarray_log_set_t * sslarray_log_set_new() {
    sslarray_log_set_t *sslarray_log_set;

    if ((sslarray_log_set = (sslarray_log_set_t *) malloc(sizeof (sslarray_log_set_t))) == NULL) {
      perror("Could not initialize the sslarray_log set");
      return NULL;
    }

    if ((sslarray_log_set->log_entries = (sslarray_log_entry_t *) malloc(SSLARRAY_LOG_SET_SIZE * sizeof (sslarray_log_entry_t))) == NULL) {
      free(sslarray_log_set);
      perror("Could not initialize the sslarray_log set");
      return NULL;
    }

    sslarray_log_set->nb_entries = 0;
    sslarray_log_set->size = SSLARRAY_LOG_SET_SIZE;

    return sslarray_log_set;
  }


  INLINED void sslarray_log_set_free(sslarray_log_set_t *sslarray_log_set) {
    free(sslarray_log_set->log_entries);
    free(sslarray_log_set);
  }


  INLINED sslarray_log_set_t * sslarray_log_set_empty(sslarray_log_set_t *sslarray_log_set) {
    sslarray_log_set->nb_entries = 0;
    return sslarray_log_set;
  }

  INLINED sslarray_log_entry_t * sslarray_log_set_entry(sslarray_log_set_t *sslarray_log_set) {
    if (sslarray_log_set->nb_entries == sslarray_log_set->size) {
      printf("LOCK set max sized (%d)\n", sslarray_log_set->size);
      unsigned int new_size = 2 * sslarray_log_set->size;
      sslarray_log_entry_t *temp;
      if ((temp = (sslarray_log_entry_t *) realloc(sslarray_log_set->log_entries, new_size * sizeof (sslarray_log_entry_t))) == NULL) {
	sslarray_log_set_free(sslarray_log_set);
	printf("Could not resize the sslarray_log set\n");
	return NULL;
      }

      sslarray_log_set->log_entries = temp;
      sslarray_log_set->size = new_size;
    }

    return &sslarray_log_set->log_entries[sslarray_log_set->nb_entries++];
  }

  INLINED void sslarray_log_set_insert(sslarray_log_set_t *sslarray_log_set, entry_addr_t address) {
    sslarray_log_entry_t *we = sslarray_log_set_entry(sslarray_log_set);
    *we = address;
  }

#ifdef	__cplusplus
}
#endif

#endif	/* LOG_H */

