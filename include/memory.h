#ifndef _MEMORY_H
#define _MEMORY_H

#include <hardware.h>
#include <mmu.h>

/*
 * Split usable memory into two parts: one for page-size memory allocation, one
 * for arbitrary-size memory allocation.
 */

/* usable memory region: 256M-1024M */
enum {PAGE_POOL_SIZE = (256 *(1<<20)), MEM_POOL_SIZE = (512 *(1<<20))};
enum {MEM_POOL_START = (PAGE_OFFSET + RAM_ADDR_START + (256 *(1<<20))),
	MEM_POOL_END = (MEM_POOL_START + MEM_POOL_SIZE),
	PAGE_POOL_START = MEM_POOL_END,
	PAGE_POOL_END = PAGE_POOL_START + PAGE_POOL_SIZE};

void init_kmalloc_free(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);

void kfree(void *ptr);

void mem_init(void);

void *get_free_pages(unsigned int order);

void *get_zeroed_pages(unsigned int order);

void free_pages(void *addr, unsigned int order);
#endif
