/*
 *   File: tmN.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: simple TM benchmarks
 *   This file is part of TM2C
 *
 *   Copyright (C) 2013  Vasileios Trigonakis
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/*
 * testing the assignement of addr to dsl cores
 */

#include <assert.h>

#include "tm2c.h"

#define SIS_SIZE 65536

size_t mem_size = SIS_SIZE * sizeof(int32_t);
size_t num_dsl = 16;

static inline nodeid_t
get_responsible_node(tm_intern_addr_t addr) 
{
#if defined(PGAS)
  return addr >> PGAS_DSL_MASK_BITS;
#else	 /* !PGAS */
#  if (ADDR_TO_DSL_SEL == 0)
  return hash_tw(addr >> ADDR_SHIFT_MASK) % num_dsl;
#  elif (ADDR_TO_DSL_SEL == 1)
  return ((addr) >> ADDR_SHIFT_MASK) % num_dsl;
#  endif
#endif	/* PGAS */
}

void* mainthread(void* args) {
 ///TM2C_INIT_THREAD
  PRINTD("Initializing child %u", rank);
  TM2C_ID = *((nodeid_t*) args);
  free(args);
  ssmp_mem_init(TM2C_ID, TM2C_NUM_NODES);

  // Now, pin the process to the right core (NODE_ID == core id)
  int place = rank_to_core[rank];
  cpu_set_t mask;
  cpu_set_t cpuset;
  pthread_t thread = pthread_self();
  CPU_ZERO(&cpuset);
  CPU_SET(place, &cpuset);
  if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
	  fprintf(stderr, "Problem with setting thread affinity\n");
	  exit(3);
  }
  //resume to tm2c_init_system
}
int
main(int argc, char **argv)
{
  TM2C_INIT;
/**must fork here I believe*/
  /*for(rank = 1; rank < TM2C_NUM_NODES; rank++) {
      PRINTD("Forking child %u", rank);
	  uint8_t *id = malloc(sizeof(uint8_t));
	  *id = (uint8_t) rank;
	  if (0 < pthread_create(&threads[rank], NULL, mainthread, (void*) id)) {
		  P("Failure in pthread_create():\n%s", strerror(errno));
	  }
  }
  pthread_exit(NULL);*/

  printf("init without bug\n");
  BARRIER;
  if (argc > 1)
    {
      mem_size = atoi(argv[1]) * sizeof(int32_t);
    }

  if (argc > 1)
    {
      num_dsl = atoi(argv[2]);
    }

  ONCE
    {
      printf("memory size in bytes: %lu, words: %lu\n  emulating %lu dsl nodes\n",
	     ((LU) mem_size), ((LU) mem_size / sizeof(int)), (LU) num_dsl);
    }
  int32_t *mem = (int32_t *) sys_shmalloc(SIS_SIZE * sizeof(int32_t));
  assert(mem != NULL);

  BARRIER;

  ONCE
    {
      PRINT("proc        | number of addr");

      size_t num_resp[TM2C_MAX_PROCS] = {0};

      uint32_t a;
      for (a = 0; a < SIS_SIZE; a++)
	{
	  tm_intern_addr_t ia = to_intern_addr(mem + a);
	  num_resp[get_responsible_node(ia)]++;
	}

      for (a = 0; a < TM2C_MAX_PROCS; a++)
	{
	  if (num_resp[a])
	    {
	      printf("%-20u%lu\n", a, (LU) num_resp[a]);
	    }
	}
    }

  BARRIER;

  sys_shfree((void*) mem);

  TM_END;

  EXIT(0);
}
