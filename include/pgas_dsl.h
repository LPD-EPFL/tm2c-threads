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

#define PGAS_DSL_MASK_BITS 28
#define PGAS_DSL_ADDR_MASK 0x000000000FFFFFFF
#define PGAS_DSL_GLOBAL_ADDR_MASK 0x0000000FFFFFFFFF
#define PGAS_DSL_SIZE_NODE (0x1LL<<PGAS_DSL_MASK_BITS)

extern void pgas_dsl_init();

extern inline int64_t pgas_dsl_read(uint64_t offset);
inline int64_t* pgas_dsl_readp(uint64_t offset);
extern inline int32_t pgas_dsl_read32(uint64_t offset);

extern inline void pgas_dsl_write(uint64_t offset, int64_t val);
extern inline void pgas_dsl_write32(uint64_t offset, int32_t val);

#endif	/* PGAS_DSL_H */

