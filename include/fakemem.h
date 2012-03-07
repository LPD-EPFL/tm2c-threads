#ifndef _FAKEMEM_H
#define _FAKEMEM_H

void* fakemem_malloc(size_t size);
void fakemem_free(void* ptr);
size_t fakemem_offset(void* ptr);

#endif
