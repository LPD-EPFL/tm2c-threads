#include "pgas_dsl.h"

static void* pgas_dsl_mem;

void 
pgas_dsl_init()
{
  pgas_dsl_mem = calloc(PGAS_DSL_SIZE_NODE, 1);
  assert(pgas_dsl_mem != NULL);
  PRINT(" <><> allocated %lld B = %lld KiB = %lld MiB for pgas mem", 
	PGAS_DSL_SIZE_NODE, PGAS_DSL_SIZE_NODE / 1024, PGAS_DSL_SIZE_NODE / (1024 * 1024));
}

void
pgas_dsl_term()
{
  free(pgas_dsl_mem);
}

inline int64_t
pgas_dsl_read(uint64_t offset)
{
  return *(int64_t*) (pgas_dsl_mem + offset);
}

inline int64_t*
pgas_dsl_readp(uint64_t offset)
{
  return (int64_t*) (pgas_dsl_mem + offset);
}


inline int32_t
pgas_dsl_read32(uint64_t offset)
{
  return *(int32_t*) (pgas_dsl_mem + offset);
}


inline void
pgas_dsl_write(uint64_t offset, int64_t val)
{
  *(int64_t*) (pgas_dsl_mem + offset) = val;
}

inline void
pgas_dsl_write32(uint64_t offset, int32_t val)
{
  *(uint32_t*) (pgas_dsl_mem + offset) = val;
}
