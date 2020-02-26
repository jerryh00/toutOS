#ifndef _TEST_MEM_ALLOC_H
#define _TEST_MEM_ALLOC_H

#include <list.h>
#include <string.h>

#if defined IN_KERNEL
#include <sched.h>
#include <memory.h>
#include <printk.h>
#define PRINT printk
#else
#include <stdio.h>
#define PRINT printf
#endif

enum testmem_blk_type { MALLOC_BLK, PAGE_ALLOC_BLK };

struct test_mem_alloc {
	enum testmem_blk_type type;
	void *(*malloc)(size_t size);
	void (*free)(void *ptr);
	void *(*get_free_pages)(unsigned int order);
	void (*free_pages)(void *addr, unsigned int order);
};

static inline void init_test_mem_alloc(struct test_mem_alloc *p,
			 enum testmem_blk_type type,
			 void *(*malloc)(size_t size),
			 void (*free)(void *ptr),
			 void *(*get_free_pages)(unsigned int order),
			 void (*free_pages)(void *addr, unsigned int order))
{
	p->type = type;
	p->malloc = malloc;
	p->free = free;
	p->get_free_pages = get_free_pages;
	p->free_pages = free_pages;
}

static int check_mem_ok(void *mem, int c, size_t n)
{
	char *mem_char = (char *)mem;
	int i;

	for (i = 0; i < n; i++) {
		if (mem_char[i] != c) {
			PRINT("%s failed: mem=%p, c=%x, n=%u\n",
			       __FUNCTION__, mem, c, n);
			return false;
		}
	}

	return true;
}

struct testmem_blk {
	struct list_head node;
	int type;
	uint8_t *addr;
	size_t size;
	uint8_t pattern;
};

static struct testmem_blk *get_testmem_blk(struct test_mem_alloc *p,
					   size_t size, uint8_t pattern)
{
	struct testmem_blk *tmb;
	uint8_t *addr;

	tmb = p->malloc(sizeof(*tmb));
	if (tmb == NULL) {
		return NULL;
	}

	if (p->type == MALLOC_BLK) {
		addr = p->malloc(size);
	} else {
		addr = p->get_free_pages(get_order(size));
	}
	if (addr == NULL) {
		p->free(tmb);
		return NULL;
	}

#ifdef DEBUG_MEMORY_ALLOC
#ifdef IN_KERNEL
	PRINT("%s: pid=%d, type=%d, addr=%p, size=%d\n", __FUNCTION__,
	       get_current_proc()->pid, p->type, addr, size);
#else
	PRINT("%s: type=%d, addr=%p, size=%d\n", __FUNCTION__,
	      p->type, addr, size);
#endif
#endif
	memset(addr, pattern, size);

	INIT_LIST_HEAD(&tmb->node);
	tmb->type = p->type;
	tmb->addr = addr;
	tmb->size = size;
	tmb->pattern = pattern;

	return tmb;
}

static void put_testmem_blk(struct test_mem_alloc *p,
			    const struct testmem_blk *tmb)
{
	if (tmb->type == MALLOC_BLK) {
		p->free(tmb->addr);
	} else {
		p->free_pages(tmb->addr, get_order(tmb->size));
	}
}

static inline int test_mem(struct test_mem_alloc *p)
{
	size_t size;
	uint8_t pattern;
	LIST_HEAD(testmem_list);
	struct testmem_blk *tmb;
	struct testmem_blk *tmb_dummy;
	int ret = 0;
	int max_size = 32 * PAGE_SIZE;

	size = 1;
	while (true) {
		if (size > max_size) {
			break;
		}
		pattern = (size & 0xFF);

		tmb = get_testmem_blk(p, size, pattern);
		if (tmb != NULL) {
			list_add_tail(&tmb->node, &testmem_list);
		} else {
			break;
		}

		size += 311;
#ifdef IN_KERNEL
		schedule();
#endif
	}
	list_for_each_entry_safe(tmb, tmb_dummy, &testmem_list, node) {
		if (ret == 0) {
			if (!check_mem_ok(tmb->addr, tmb->pattern, tmb->size)) {
				PRINT("test_mem failed\n");
				ret = -1;
			}
		}
		put_testmem_blk(p, tmb);
		list_del(&tmb->node);
		p->free(tmb);
#ifdef IN_KERNEL
		schedule();
#endif
	}

	return ret;
}

#endif
