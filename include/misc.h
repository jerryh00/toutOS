#ifndef __MISC_H__
#define __MISC_H__

#include "arch.h"

/* The lower bits of mask should be zero. */
#define is_pointer_aligned(pointer, mask)\
	(((uintptr_t)pointer & (uintptr_t)mask) == (uintptr_t)pointer)

#define NUM_ELEMENTS(array) (sizeof(array) / sizeof(array[0]))

void cpu_idle(void);

void dump_stack(void);

struct timestamp {
	unsigned long sec;  /* seconds. */
	unsigned long usec; /* micro-seconds. */
};

struct timestamp get_timestamp(void);

void udelay(unsigned long delay);
void mdelay(unsigned long delay);

#define panic(fmt, arg...) \
	do { printk(fmt, ##arg); \
		disable_irq(); \
		dump_stack(); \
		assert(0); \
	} while (0)

#define PAGE_ADDR(addr) (((u64)(addr)) & ~(PAGE_SIZE - 1UL))
#define IN_PAGE_OFFSET(addr) (((u64)(addr)) & (PAGE_SIZE - 1UL))

#define UPPER_PAGE_ADDR(addr) (((u64)(addr) + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL))

void dump_mem_in_hex_byte(const void *start_addr, size_t len);

void dump_mem_in_u64(const void *start_addr, size_t len);

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

#endif
