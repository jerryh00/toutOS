#ifndef _MMU_H
#define _MMU_H

void enable_paging(void);

#ifdef MMU_BY_BLOCK
#define VA_BITS 32
#else
#define VA_BITS 39
#endif

/*
 * Re. Linux, the linear mapped address starts at the half of the kernel
 * virtual address space
 */
#define VA_START                (0xffffffffffffffffUL << VA_BITS)
#define PAGE_OFFSET             (0xffffffffffffffffUL << (VA_BITS - 1))

#define USER_DEVICE_MAP_START 0x100000000

#define __va(x) ((void *)((unsigned long)(x) + PAGE_OFFSET))
#define __pa(x) ((void *)((unsigned long)(x) - PAGE_OFFSET))
#define __uva(x) ((void *)((unsigned long)(x) + USER_DEVICE_MAP_START))

struct memory_map {
	uint64_t phy_addr;
	uint64_t virt_addr;
	uint64_t size;
	uint64_t attrs;
};

#endif
