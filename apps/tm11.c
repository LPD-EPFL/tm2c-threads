/*
 * testing the assignement of addr to dsl cores
 */

#include <assert.h>

#include "tm.h"

#define SIS_SIZE 1024

size_t mem_size = SIS_SIZE * sizeof(int32_t);

static inline nodeid_t
get_responsible_node(tm_intern_addr_t addr) 
{
  return dsl_nodes[((addr) >> 6) % NUM_DSL_NODES];
}

MAIN(int argc, char **argv) {
  if (argc > 1)
    {
      mem_size = atoi(argv[1]) * sizeof(int32_t);
    }

  PRINT("memory size in bytes: %lu\t, words: ", mem_size);

  TM_INIT;

  int32_t *mem = (int32_t *) sys_shmalloc(SIS_SIZE * sizeof(int32_t));
  assert(mem != NULL);

  BARRIER;

  ONCE
    {
      PRINT("addr               | resp");

      uint32_t a;
      for (a = 0; a < SIS_SIZE; a++)
	{
	  tm_intern_addr_t ia = to_intern_addr(mem + a);
	  PRINT("%-18p %02d", (void*) ia, get_responsible_node(ia));
	}
    }


  TM_END;

  EXIT(0);
}
