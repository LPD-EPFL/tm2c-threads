/* 
 * File:   pgas_dsl.h
 * Author: trigonak
 *
 * header file for the PGAS memory model (service core side) implementation on TM2C
 * 
 * Created on Novermber 1, 2012, 12:46
 */
#ifndef PGAS_DSL_H
#define	PGAS_DSL_H

#include "common.h"
#include <assert.h>

#define PGAS_DSL_MASK_BITS 25
#  if __WORDSIZE == 32
#warning "32 word"
#define PGAS_DSL_ADDR_MASK        0x01FFFFFF
#define PGAS_DSL_GLOBAL_ADDR_MASK 0xFFFFFFFF
#define PGAS_DSL_SIZE_NODE (0x1<<PGAS_DSL_MASK_BITS)
#else  /* !SCC */
#define PGAS_DSL_ADDR_MASK        0x0000000001FFFFFF
#define PGAS_DSL_GLOBAL_ADDR_MASK 0x00000001FFFFFFFF
#define PGAS_DSL_SIZE_NODE (0x1LL<<PGAS_DSL_MASK_BITS)
#endif	/* SCC */

extern void pgas_dsl_init();

extern int64_t pgas_dsl_read(uint64_t offset);
inline int64_t* pgas_dsl_readp(uint64_t offset);
extern int32_t pgas_dsl_read32(uint64_t offset);

extern void pgas_dsl_write(uint64_t offset, int64_t val);
extern void pgas_dsl_write32(uint64_t offset, int32_t val);

#endif	/* PGAS_DSL_H */

