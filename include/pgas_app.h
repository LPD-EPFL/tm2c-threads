/* 
 * File:   pgas_app.h
 * Author: trigonak
 *
 * header file for the PGAS memory model (app core side) implementation on TM2C
 * 
 * Created on Novermber 1, 2012, 14:46
 */
#ifndef PGAS_APP_H
#define	PGAS_APP_H

#include "common.h"
#include "pgas_dsl.h"

extern nodeid_t pgas_app_my_resp_node;
extern nodeid_t pgas_app_my_resp_node_real;
extern size_t pgas_dsl_size_node;

extern void pgas_app_init();

extern inline size_t pgas_app_addr_offs(void* addr);
extern inline void* pgas_app_addr_from_offs(size_t offs);


extern inline void* pgas_app_alloc(size_t size);
extern void** pgas_app_alloc_rr(size_t num_elems, size_t size_elem);
extern inline void* pgas_app_free(void* addr);

#endif	/* PGAS_APP_H */
