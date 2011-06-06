/****************************************************************************************
 * Put data into communication buffer. 
 ****************************************************************************************
 *
 * Author: Stefan Lankes, Carsten Clauss
 *         Chair for Operating Systems, RWTH Aachen University
 * Date:   11/03/2010
 *
 ****************************************************************************************
 * 
 * Written by the Chair for Operating Systems, RWTH Aachen University
 * 
 * NO Copyright (C) 2010, Stefan Lankes, Carsten Clauss,
 * consider these trivial functions to be public domain.
 * 
 * These functions are distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */ 

#ifndef __SCC_MEMCPY_H_
#define __SCC_MEMPCY_H_

/*
 * A write access, which cache line is not present, doesn't perform (on the
 * current SCC architecture) a cache line fill. Therefore, the core writes 
 * in this case directly to the memory. 
 *
 * The following function copies from the on-die  memory (MPB) to the off-die
 * memory and prefetchs its destintation. Therefore, the  function avoids the 
 * bad behavior of a "write miss".
 */ 
inline static void *memcpy_from_mpb(void *dest, const void *src, size_t count)
{
	int h, i, j, k, l, m;

	asm volatile ("cld;\n\t"
		      "1: cmpl $0, %%eax ; je 2f\n\t"
		      "movl (%%edi), %%edx\n\t"
		      "movl 0(%%esi), %%ecx\n\t"
		      "movl 4(%%esi), %%edx\n\t"
		      "movl %%ecx, 0(%%edi)\n\t"
		      "movl %%edx, 4(%%edi)\n\t"
		      "movl 8(%%esi), %%ecx\n\t"
		      "movl 12(%%esi), %%edx\n\t"
		      "movl %%ecx, 8(%%edi)\n\t"
		      "movl %%edx, 12(%%edi)\n\t"
		      "movl 16(%%esi), %%ecx\n\t"
		      "movl 20(%%esi), %%edx\n\t"
		      "movl %%ecx, 16(%%edi)\n\t"
		      "movl %%edx, 20(%%edi)\n\t"
		      "movl 24(%%esi), %%ecx\n\t"
		      "movl 28(%%esi), %%edx\n\t"
		      "movl %%ecx, 24(%%edi)\n\t"
		      "movl %%edx, 28(%%edi)\n\t"
		      "addl $32, %%esi\n\t"
		      "addl $32, %%edi\n\t"
		      "dec %%eax ; jmp 1b\n\t"
		      "2: movl %%ebx, %%ecx\n\t"
		      "movl (%%edi), %%edx\n\t"
		      "andl $31, %%ecx\n\t"
		      "rep ; movsb\n\t":"=&a" (h), "=&D"(i), "=&S"(j), "=&b"(k),
		      "=&c"(l), "=&d"(m)
		      :"0"(count / 32), "1"(dest), "2"(src),
		      "3"(count):"memory");

	return dest;
}


/*
 * If the destination is located on on-die memory (MPB), classical prefetching 
 * techniques will be used to increase the performance.
 */
inline static void *memcpy_to_mpb(void *dest, const void *src, size_t count)
{
	int i, j, k, l;

	/* 
	 * We use the floating point registers to
	 * prefetch up to 4096 = (DCACE_SIZE (16KB) / 4) bytes.
	 */
	asm volatile ("cmpl $63,%%ecx\n\t"
		      "jbe 1f\n\t"
		      "4: pushl %%ecx\n\t"
		      "cmpl $4096, %%ecx\n\t"
		      "jbe 2f\n\t"
		      "movl $4096,%%ecx\n\t"
		      "2: subl %%ecx,0(%%esp)\n\t"
		      "cmpl $256,%%ecx\n\t"
		      "jb 5f\n\t"
		      "pushl %%esi\n\t"
		      "pushl %%ecx\n\t"
		      ".align 4,0x90\n\t"
		      "3: movl 0(%%esi),%%eax\n\t"
		      "movl 32(%%esi),%%eax\n\t"
		      "movl 64(%%esi),%%eax\n\t"
		      "movl 96(%%esi),%%eax\n\t"
		      "movl 128(%%esi),%%eax\n\t"
		      "movl 160(%%esi),%%eax\n\t"
		      "movl 192(%%esi),%%eax\n\t"
		      "movl 224(%%esi),%%eax\n\t"
		      "addl $256,%%esi\n\t"
		      "subl $256,%%ecx\n\t"
		      "cmpl $256,%%ecx\n\t"
		      "jae 3b\n\t"
		      "popl %%ecx\n\t"
		      "popl %%esi\n\t"
		      ".align 2,0x90\n\t"
		      "5: fildq 0(%%esi)\n\t"
		      "fildq 8(%%esi)\n\t"
		      "fildq 16(%%esi)\n\t"
		      "fildq 24(%%esi)\n\t"
		      "fildq 32(%%esi)\n\t"
		      "fildq 40(%%esi)\n\t"
		      "fildq 48(%%esi)\n\t"
		      "fildq 56(%%esi)\n\t"
		      "fistpq 56(%%edi)\n\t"
		      "fistpq 48(%%edi)\n\t"
		      "fistpq 40(%%edi)\n\t"
		      "fistpq 32(%%edi)\n\t"
		      "fistpq 24(%%edi)\n\t"
		      "fistpq 16(%%edi)\n\t"
		      "fistpq 8(%%edi)\n\t"
		      "fistpq 0(%%edi)\n\t"
		      "addl $-64,%%ecx\n\t"
		      "addl $64,%%esi\n\t"
		      "addl $64,%%edi\n\t"
		      "cmpl $63,%%ecx\n\t"
		      "ja 5b\n\t"
		      "popl %%eax\n\t"
		      "addl %%eax,%%ecx\n\t"
		      "cmpl $64,%%ecx\n\t"
		      "jae 4b\n\t"
		      "1: movl %%ecx,%%eax\n\t"
		      "shrl $2,%%ecx\n\t"
		      "cld ; rep ; movsl\n\t"
		      "movl %%eax,%%ecx\n\t"
		      "andl $3,%%ecx\n\t"
		      "rep ; movsb\n\t":"=&c" (i), "=&D"(j), "=&S"(k), "=&a"(l)
		      :"0"(count), "1"(dest), "2"(src)
		      :"memory");

	return dest;
}

#endif
