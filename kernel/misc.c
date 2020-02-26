#include <misc.h>
#include <arch.h>
#include <printk.h>
#include <hardware.h>
#include <hw_timer.h>

extern char KERNEL_LINEAR_START[];

static int within_stack(const u64 *fp)
{
	u64 *sp;
	u64 *stack_max;
	u64 *stack_min;

	asm volatile (
		      "MOV %0, SP\n\t"
		      : "=r" (sp)
		     );
	stack_min = (u64 *)(((u64)sp & ~(PAGE_SIZE-1)));
	stack_max = (u64 *)(((u64)stack_min) + PAGE_SIZE);

	return (fp >= stack_min && fp < stack_max);
}

void dump_stack(void)
{
	unsigned long flags;
	u64 elr_el1;
	int core_id;
	u64 *fp;
	u32 *lr;
	u32 *branch_addr;

	core_id = get_cpu_core_id();

	asm volatile (
		      "MOV %0, X29\n\t"
		      : "=r" (fp)
		     );
	local_irq_save(flags);

	printk("core %d: dump_stack() begin\n", core_id);
	asm volatile (
		      "MRS %x[reg_elr_el1], ELR_EL1\n\t"
		      :[reg_elr_el1] "=r" (elr_el1)
		     );
	printk("core %d: elr_el1=%p\n", core_id, elr_el1);

	while (within_stack(fp)) {
		lr = (u32 *)(*(fp + 1));
		branch_addr = lr - 1;
		if (branch_addr < (u32 *)KERNEL_LINEAR_START) {
			break;
		} else {
			printk("%p\n", branch_addr);
		}
		fp = (u64 *)(*fp);
	}
	printk("core %d: dump_stack() end\n", core_id);

	local_irq_restore(flags);
}

void cpu_idle(void)
{
	asm volatile (
		      "loop_idle:\n\t"
		      "wfi\n\t"
		      "b loop_idle\n\t"
		      );
}

struct timestamp get_timestamp(void)
{
	struct timestamp ts;
	uint64_t timer_count = read_reg(CNTPCT_EL0);

	ts.sec = timer_count / CNTFRQ_EL0_VALUE;
	ts.usec = (timer_count % CNTFRQ_EL0_VALUE) * 1000 * 1000
		/ CNTFRQ_EL0_VALUE;

	return ts;
}

void udelay(unsigned long delay)
{
	uint64_t timer_count;
	uint64_t timer_count_delta;
	uint64_t timer_count_end;

	timer_count = read_reg(CNTPCT_EL0);
	timer_count_delta = CNTFRQ_EL0_VALUE * delay / (1000 * 1000);

	timer_count_end = timer_count + timer_count_delta;

#ifdef DEBUG_DELAY
	printk("timer_count=%u, timer_count_end=%u\n",
	       timer_count, timer_count_end);
#endif
	while (timer_count < timer_count_end) {
		timer_count = read_reg(CNTPCT_EL0);
	}
}

void mdelay(unsigned long delay)
{
	udelay(1000 * delay);
}

void dump_mem_in_hex_byte(const void *start_addr, size_t len)
{
	size_t i;
	size_t lines;
	size_t remains;
	const uint8_t *addr = (const uint8_t *)start_addr;

	if (start_addr == NULL) {
		printk("%s: start_addr is null\n", __FUNCTION__);
		return;
	}

	lines = len / 4;
	remains = len % 4;

	for (i = 0; i < lines; i++) {
		printk("%x %x %x %x\n", addr[0], addr[1], addr[2], addr[3]);
		addr += 4;
	}
	if (remains == 1) {
		printk("%x\n", addr[0]);
	} else if (remains == 2) {
		printk("%x %x\n", addr[0], addr[1]);
	} else if (remains == 3) {
		printk("%x %x %x\n", addr[0], addr[1], addr[2]);
	}
}

void dump_mem_in_u64(const void *start_addr, size_t len)
{
	size_t i;
	size_t num_u64;
	size_t lines;
	size_t remains;
	const u64 *addr = (const u64 *)start_addr;

	if (start_addr == NULL) {
		printk("%s: start_addr is null\n", __FUNCTION__);
		return;
	}

	num_u64 = len / (sizeof (u64));
	lines = num_u64 / 4;
	remains = num_u64 % 4;

	for (i = 0; i < lines; i++) {
		printk("%p %p %p %p\n", addr[0], addr[1], addr[2], addr[3]);
		addr += 4;
	}
	if (remains == 1) {
		printk("%p\n", addr[0]);
	} else if (remains == 2) {
		printk("%p %p\n", addr[0], addr[1]);
	} else if (remains == 3) {
		printk("%p %p %p\n", addr[0], addr[1], addr[2]);
	}
}
