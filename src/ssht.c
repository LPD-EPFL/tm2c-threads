#include "ssht.h"

ssht_hashtable_t 
ssht_new() {
  ssht_hashtable_t hashtable;
  if (posix_memalign((void**) &hashtable, CACHE_LINE_SIZE, NUM_BUCKETS * sizeof(bucket_t)) != 0)
    {
      perror("posix_memalign failed\n");
      EXIT(-1);
    }

  assert(hashtable != NULL);
  assert((intptr_t) hashtable % CACHE_LINE_SIZE == 0);
  assert(sizeof(ssht_rw_entry_t) % CACHE_LINE_SIZE == 0);
  assert(sizeof(bucket_t) % CACHE_LINE_SIZE == 0);

  uint64_t* ht_uint64_t = (uint64_t*) hashtable;
  uint32_t i;

  for (i = 0; i < (NUM_BUCKETS * sizeof(bucket_t)) / sizeof(uint64_t); i++)
    {
      ht_uint64_t[i] = 0;
    }

  for (i = 0; i < NUM_BUCKETS; i++) {
    uint32_t j;
    for (j = 0; j < ENTRY_PER_CL; j++) {
      hashtable[i].entry[j].writer = SSHT_NO_WRITER;
    }
  }

  return hashtable;
}

void 
bucket_print(bucket_t *bu) {
  bucket_t* btmp = bu;
  do 
    {
      uint32_t j;
      for (j = 0; j < ADDR_PER_CL; j++) 
	{
	  printf("%p:%2d/%d|", (void *)btmp->addr[j], btmp->entry[j].nr, btmp->entry[j].writer);
	}
      btmp = btmp->next;
      printf("|");
    } 
  while (btmp != NULL);
  printf("\n");
}


CONFLICT_TYPE 
bucket_insert_r(bucket_t* bu, ssht_log_set_t* log, uint32_t id, uintptr_t addr) 
{
  uint32_t i;
  bucket_t *btmp = bu;
  do 
    {
      for (i = 0; i < ADDR_PER_CL; i++) 
	{
	  if (btmp->addr[i] == addr) 
	    {
	      ssht_rw_entry_t *e = btmp->entry + i;
	      if (e->writer != SSHT_NO_WRITER) 
		{
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
		  if (!contention_manager_raw_waw(id, (uint16_t) e->writer, READ_AFTER_WRITE)) 
		    {
		      return READ_AFTER_WRITE;
		    }
#else
		  return READ_AFTER_WRITE;
#endif	/* NOCM */
		}

	      e->nr++;
	      e->reader[id] = 1;
	      btmp->addr[i] = addr;
	      ssht_log_set_insert(log, btmp->addr + i, e);
	      return NO_CONFLICT;
	    }
	}

      btmp = btmp->next;
    } 
  while (btmp != NULL);

  btmp = bu;

  do 
    {
      for (i = 0; i < ADDR_PER_CL; i++) 
	{
	  if (btmp->addr[i] == 0) {
	    ssht_rw_entry_t *e = btmp->entry + i;
	    e->nr++;
	    e->reader[id] = 1;
	    btmp->addr[i] = addr;
	    ssht_log_set_insert(log, btmp->addr + i, e);
	    return NO_CONFLICT;
	  }
	}

      if (btmp->next == NULL) 
	{
	  btmp->next = ssht_bucket_new();
	}

      btmp = btmp->next;
    } 
  while (1);
}  

CONFLICT_TYPE 
bucket_insert_w(bucket_t* bu, ssht_log_set_t* log, uint32_t id, uintptr_t addr) 
{
  uint32_t i;
  bucket_t *btmp = bu;
  do 
    {
      for (i = 0; i < ADDR_PER_CL; i++) 
	{
	  if (btmp->addr[i] == addr) 
	    {
	      ssht_rw_entry_t* e = btmp->entry + i;
	      if (e->writer != SSHT_NO_WRITER) {
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
		if (contention_manager_raw_waw(id, e->writer, WRITE_AFTER_WRITE)) 
		  {
		    e->writer = id;
		    btmp->addr[i] = addr;
		    ssht_log_set_insert(log, btmp->addr + i, e);
		    return NO_CONFLICT;
		  }
		else 
		  {
		    return WRITE_AFTER_WRITE;
		  }
#else
		return WRITE_AFTER_WRITE;
#endif	/* NOCM */
	      }
	      else if (e->nr > 1 || e->reader[id] == 0) 
		{
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
		  if (contention_manager_war(id, e->reader, WRITE_AFTER_READ))
		    {
		      e->writer = id;
		      btmp->addr[i] = addr;
		      ssht_log_set_insert(log, btmp->addr + i, e);
		      return NO_CONFLICT;
		    }
		  else
		    {
		      return WRITE_AFTER_READ;
		    }
#else
		  return WRITE_AFTER_READ;
#endif	/* NOCM */
		}
	    }
	}
    
      btmp = btmp->next;
    }
  while (btmp != NULL);

  btmp = bu;
  do
    {
      for (i = 0; i < ADDR_PER_CL; i++)
	{
	  if (btmp->addr[i] == 0) {
	    ssht_rw_entry_t* e = btmp->entry + i;
	    e->writer = id;
	    btmp->addr[i] = addr;
	    ssht_log_set_insert(log, btmp->addr + i, e);
	    return NO_CONFLICT;
	  }
	}

      if (btmp->next == NULL)
	{
	  btmp->next = ssht_bucket_new();
	}

      btmp = btmp->next;
    } 
  while (1);
}

CONFLICT_TYPE 
ssht_insert_w_test(ssht_hashtable_t ht, uint32_t id, uintptr_t addr)
{
  uint32_t bu = hash_tw((addr>>2) % UINT_MAX);
  bucket_t *btmp = ht + (bu % NUM_OF_BUCKETS);

  uint32_t i;
  do 
    {
      for (i = 0; i < ADDR_PER_CL; i++)
	{
	  if (btmp->addr[i] == addr) {
	    ssht_rw_entry_t* e = btmp->entry + i;
	    if (e->writer != SSHT_NO_WRITER)
	      {
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
		if (contention_manager_raw_waw(id, e->writer, WRITE_AFTER_WRITE))
		  {
		    btmp->addr[i] = 0;
		    return NO_CONFLICT;
		  }
		else 
		  {
		    return WRITE_AFTER_WRITE;
		  }
#else
		return WRITE_AFTER_WRITE;
#endif	/* NOCM */
	      }
	    else 
	      {
#ifndef NOCM 			/* if any other CM (greedy, wholly, faircm) */
		if (contention_manager_war(id, e->reader, WRITE_AFTER_READ)) 
		  {
		    btmp->addr[i] = 0;
		    return NO_CONFLICT;
		  }
		else 
		  {
		    return WRITE_AFTER_READ;
		  }
#else
		return WRITE_AFTER_READ;
#endif	/* NOCM */
	      }
	  }
	}
    
      btmp = btmp->next;
    } 
  while (btmp != NULL);

  return NO_CONFLICT;
}
