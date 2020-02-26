#include <memory.h>
#include <string.h>
#include <printk.h>
#include <hardware.h>
#include <spinlock.h>

#define IN_KERNEL
#include "mm.c"

static struct mem_pool kmalloc_pool;
static struct spinlock kmalloc_lock;

static void *kernel_mock_sbrk(intptr_t increment)
{
	static void *last = (void *)MEM_POOL_START;
	static intptr_t allocated_size = 0;

	if ((allocated_size + increment >= 0) &&
			(allocated_size + increment <= MEM_POOL_SIZE)) {
		void *save_last = last;
		last += increment;
		allocated_size += increment;
		return save_last;
	} else {
		return (void *)(-1);
	}
}

void init_kmalloc_free(void)
{
	/* To suppress get_pool_malloc_total_length defined but not used warning. */
	printk("get_pool_malloc_total_length()@%p\n",
	       get_pool_malloc_total_length);
	printk("PAGE_POOL_SIZE=%u, MEM_POOL_SIZE=%u, MEM_POOL_START=%p, MEM_POOL_END=%p, PAGE_POOL_START=%p, PAGE_POOL_END=%p\n",
	       PAGE_POOL_SIZE, MEM_POOL_SIZE, MEM_POOL_START, MEM_POOL_END, PAGE_POOL_START, PAGE_POOL_END);

#ifdef QEMU_VIRT
       printk("test RAM END=%d\n", (*(uint64_t *)(PAGE_POOL_END - 8)));
#endif

	init_mm(&kmalloc_pool,
		&kernel_mock_sbrk,
		&kmalloc_lock,
		(void (*)(void *lock))&spin_lock_init,
		(void (*)(void *lock))&spin_lock,
		(void (*)(void *lock))&spin_unlock);
}

void *kmalloc(size_t size)
{
	return alloc_mem(&kmalloc_pool, size);
}

void *kzalloc(size_t size)
{
	void *mem;

	mem = kmalloc(size);

	if (mem != NULL) {
		memset(mem, 0, size);
	}

	return mem;
}

void kfree(void *user_start)
{
	free_mem(&kmalloc_pool, user_start);
}

#define MAX_NUM_PAGES (PAGE_POOL_SIZE / PAGE_SIZE)
static char pages_usage[MAX_NUM_PAGES];

static int find_free_pages(unsigned int num)
{
	int i;
	int first_page;
	int last_page = -1;

#ifdef DEBUG_MEMORY_ALLOCATION
	printk("num=%d\n", num);
#endif
	if (num == 0) {
		return -1;
	}

	for (int free_count = 0, i = 0; i < MAX_NUM_PAGES; i++) {
		if (pages_usage[i] == 0) {
			free_count++;
			if (free_count == num) {
				last_page = i;
				break;
			}
		} else {
			free_count = 0;
		}
	}

	if (last_page < 0) {
		return -1;
	}

	first_page = last_page - num + 1;
#ifdef DEBUG_MEMORY_ALLOCATION
	printk("first_page=%d, last_page=%d\n", first_page, last_page);
#endif
	for (i = first_page; i <= last_page; i++) {
		pages_usage[i] = 1;
	}

	return first_page;
}

void *get_free_pages(unsigned int order)
{
	unsigned int num = (1 << order);

	int first_page = find_free_pages(num);

	if (first_page >= 0) {
		return (void *)(unsigned long)(PAGE_POOL_START
					       + PAGE_SIZE * first_page);
	} else {
		return NULL;
	}
}

void *get_zeroed_pages(unsigned int order)
{
	void *pages;

	pages = get_free_pages(order);

	if (pages != NULL) {
		memset(pages, 0, (PAGE_SIZE * (1 << order)));
	}

	return pages;
}

void free_pages(void *addr, unsigned int order)
{
	int i;
	int first_page;
	int num = (1 << order);

	if ((unsigned long)addr < PAGE_POOL_START
			|| (unsigned long)addr >= PAGE_POOL_END) {
		printk("Error in %s: addr=%p\n", __FUNCTION__, addr);
		return;
	}

	if (((unsigned long)addr & (PAGE_SIZE - 1)) != 0) {
		printk("Error in %s: addr=%p is not page-aligned.\n",
		       __FUNCTION__, addr);
		return;
	}

	first_page = ((unsigned long)addr - PAGE_POOL_START) / PAGE_SIZE;
	for (i = 0; i < num; i++) {
		if (pages_usage[first_page + i] != 1) {
			printk("Error in %s: addr=%p, order=%u, first_page=%d, i=%d, pages_usage=%d\n",
			       __FUNCTION__, addr, order, first_page, i, pages_usage[first_page + i]);
			assert(0);
		}
		pages_usage[first_page + i] = 0;
	}
}
