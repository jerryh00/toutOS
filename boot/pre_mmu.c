#include <hardware.h>

extern struct task_struct swapper_task_struct;
/* Put the stack in data section so it will not cleared by clear_bss(). */
/* Init the thread_info struct at the start of stack. */
uint64_t swapper_stacks[NUM_CPUS][PAGE_SIZE/8] __attribute__
((aligned (PAGE_SIZE),section(".data"))) = {{(uint64_t)&swapper_task_struct}} ;

#ifdef TEST_PHY_MEM
#include <misc.h>
#include <print_early.h>
static int test_phy_mem_range(void *start, void *end)
{
	void **p;
	void **const min_addr = (void **)start;
	void **const max_addr = (void **)end;

	print_early("Filling memory.\n");
	print_early("min_addr=%p\n", min_addr);
	print_early("max_addr=%p\n", max_addr);
	for (p = min_addr; p < max_addr; p++) {
		*p = (void *)p;
		if (is_pointer_aligned(p, ~0xFFFFFF)) {
			print_early("%p\n", p);
		}
	}
	print_early("Fill memory finished.\n");

	print_early("Checking memory.\n");
	for (p = min_addr; p < max_addr; p++) {
		if (*p != (void *)p) {
			print_early("expected: %p\n", p);
			print_early("found: %p\n", *p);
			return -1;
		}
		if (is_pointer_aligned(p, ~0xFFFFFF)) {
			print_early("%p\n", p);
		}
	}
	print_early("Check memory finished.\n");

	return 0;
}

int test_phy_mem(void)
{
	int ret0;
	int ret1;

	ret0 = test_phy_mem_range((void *)PHYS_SDRAM_1,
				  (void *)NS_IMAGE_OFFSET);
	ret1 = test_phy_mem_range((void *)PHYS_SDRAM_2,
				   (void *)(PHYS_SDRAM_2 + PHYS_SDRAM_2_SIZE));

	if ((ret0 == 0) && (ret1 == 0)) {
		return 0;
	} else {
		return -1;
	}
}
#endif
