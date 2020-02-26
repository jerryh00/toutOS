#include <hardware.h>
#include <mmu.h>
#include <stddef.h>
#include <misc.h>
#include <printk.h>
#include <spinlock.h>
#include <uart.h>
#include <hw_timer.h>
#include <percpu.h>
#include <sched.h>
#include <string.h>
#include <memory.h>
#include <user_init.h>
#include <timer.h>
#include <mutex.h>

#define IN_KERNEL
#include <test_mem_alloc.h>

static void set_int_cpu(u64 gic_dist_base, int interrupt_id,
			int interrupt_type, uint8_t cpu_target_mask)
{
	u64 reg_addr;

	/* GICD_ISENABLERn, disable interrupt */
	reg_addr = (u64)__va(gic_dist_base + 0x100 + (interrupt_id / 32) * 4);
	*(int *)reg_addr &= ~(1 << ((interrupt_id % 32)));

	/* GICD_ITARGETSRn */
	/* for PPI int(0-31), this register is readonly. */
	if (interrupt_id >= 32) {
		reg_addr = (u64)__va(gic_dist_base + 0x800 +
				     (interrupt_id / 4) * 4 +
				     (interrupt_id % 4));
		*(char *)reg_addr = cpu_target_mask;
	}

	/* The default interrupt type is INT_TYPE_LEVEL */
	if (interrupt_type == INT_TYPE_EDGE) {
		reg_addr = (u64)__va(gic_dist_base + 0xC00 +
				     (interrupt_id / 16) * 4);
		*(int *)reg_addr |= (2 << ((interrupt_id % 16)*2));
	}

	/* GICD_ISENABLERn, enable interrupt */
	reg_addr = (u64)__va(gic_dist_base + 0x100 + (interrupt_id / 32) * 4);
	*(int *)reg_addr |= (1 << ((interrupt_id % 32)));
}

static void config_int_PMR(u64 gic_cpu_base)
{
	/* Signal to cpu any interrupt with priority higher than 0xFF */
	*(int *)__va(gic_cpu_base+4) = 0xFF;
}

static void enable_cpu_int_signal(u64 gic_cpu_base)
{
	*(int *)__va(gic_cpu_base) = 1;
}

static void enable_dist_int_send(u64 gic_dist_base)
{
	*(int *)__va(gic_dist_base) = 1;
}

static void config_gic_cpu(void)
{
	config_int_PMR(VIRT_GIC_CPU_BASE);
	enable_cpu_int_signal(VIRT_GIC_CPU_BASE);
}

static int add(int i, int j)
{
	int res = 0;

	asm volatile (
	     "ADD %w[result], %w[input_i], %w[input_j]\n\t"
	     : [result] "=r" (res)
	     : [input_i] "r" (i), [input_j] "r" (j)
	    );

	return res;
}


static unsigned long __invoke_psci_fn_hvc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

#ifdef DEBUG_GIC
static void print_gic_registers(void)
{
	int i;

#ifdef DEBUG_RTC
	printk("RTC registers:\n");
	for (i = 0; i < 0x1C; i += 4) {
		printk("%x, %p\n", i,
		       *((unsigned int *)__va(VIRT_RTC_BASE+(long)i)));
	}
#endif
	printk("distributor pending interrupts:\n");
	for (i = 0x200; i < 0x208; i += 4) {
		printk("%x, %p\n", i,
		       *((unsigned int *)__va(VIRT_GIC_DIST_BASE+(long)i)));
	}
	printk("read IAR, %p\n",
	       *((unsigned int *)__va(VIRT_GIC_CPU_BASE+0x0C)));
}
#endif

DECLARE_PER_CPU(uint64_t[MAX_NUM_INTERRUPTS], irq_trigger_count);

static void config_rtc(void)
{
	*(unsigned int *)__va(VIRT_RTC_RTCMR) = 3;	/* Match register */
	*(unsigned int *)__va(VIRT_RTC_RTCLR) = 2;	/* Load register */
	*(unsigned int *)__va(VIRT_RTC_RTCIMSC) = 1;	/* Interrupt */
}

static int kernel_init(void *p)
{
	while (true) {
#ifdef DEBUG_KERNEL_THREAD
		printk("Running in init%d\n", p);
#endif
		start_uart_tx();
		msleep(100);
	}

	return 0;
}

static int kernel_exit(void *p)
{
	printk("%s: arg=%p\n", __FUNCTION__, p);

	return -1;
}

static int test_exception(char *p)
{
	switch_to_user_mode(0, 0);

	/* TTBR1_EL1 access is allowed in EL1 only. */
	printk("user process try to read TTBR1_EL1=%p\n", read_reg(TTBR1_EL1));

	return 0;
}

#ifdef TEST_KMALLOC_GET_PAGES
static int test_get_free_pages(char *p)
{
	struct test_mem_alloc test_get_pages_struct;
	int ret;
	int run_count;

	printk("Testing get_free_pages.\n");
	init_test_mem_alloc(&test_get_pages_struct, PAGE_ALLOC_BLK,
			    kmalloc, kfree, get_free_pages, free_pages);

	run_count = 0;
	while (true) {
		run_count++;
		ret = test_mem(&test_get_pages_struct);
		if (ret < 0) {
			printk("Test get_free_pages failed, run_count=%d.\n", run_count);
			break;
		} else {
			printk("Test get_free_pages ok, run_count=%d\n", run_count);
		}
	}

	printk("get_free_pages test over.\n");

	return ret;
}

static int test_kmalloc(char *p)
{
	struct test_mem_alloc test_kmalloc_struct;
	int ret;
	int run_count;

	printk("Testing kmalloc.\n");
	init_test_mem_alloc(&test_kmalloc_struct, MALLOC_BLK, kmalloc, kfree,
			    NULL, NULL);

	run_count = 0;
	while (true) {
		run_count++;
		ret = test_mem(&test_kmalloc_struct);
		if (ret < 0) {
			printk("Test kmalloc failed, run_count=%d\n", run_count);
			break;
		} else {
			printk("Test kmalloc ok, run_count=%d\n", run_count);
		}
	}

	printk("kmalloc test over.\n");

	return ret;
}
#endif

static void clear_linear_bss(void)
{
	extern char linear_bss_start[];
	extern char linear_bss_end[];

	memset(linear_bss_start, 0, (linear_bss_end - linear_bss_start));
}

extern int _lma_user[];
extern int _vma_user[];
extern int _user_size[];

extern int _user_text_start[];
extern int _user_text_end[];
extern int _user_rodata_start[];
extern int _user_rodata_end[];
extern int _user_data_start[];
extern int _user_data_end[];
extern int _user_bss_start[];
extern int _user_bss_end[];

static int user_test(char *p)
{
	int *lma_user;
	int *vma_user;
	uint64_t user_size;

	int *user_text_start;
	int *user_text_end;
	int *user_rodata_start;
	int *user_rodata_end;
	int *user_data_start;
	int *user_data_end;
	int *user_bss_start;
	int *user_bss_end;
	int *user_stack_start = (int *)USER_STACK_START;
	int *user_stack_end = (int *)(USER_STACK_START + USER_STACK_SIZE);
	uint64_t user_init_addr;

	uint64_t *pg_dir_user_map;
	struct task_struct *current = get_current_proc();
	int ret;

	pg_dir_user_map = get_zeroed_pages(0);
	if (pg_dir_user_map == NULL) {
		printk("Memory allocation for user page directory failed\n");
		return 0;
	}
	current->pg_dir = pg_dir_user_map;
	printk("current->pg_dir=%p\n", current->pg_dir);

	/* setup page tables for userspace. */
	lma_user = __pa(_lma_user);
	vma_user = __pa(_vma_user);
	user_size = (uint64_t)__pa(_user_size);
	printk("lma_user=%p\n", lma_user);
	printk("vma_user=%p\n", vma_user);
	printk("user_size=%d\n", user_size);

	user_text_start = __pa(_user_text_start);
	user_text_end = __pa(_user_text_end);
	user_rodata_start = __pa(_user_rodata_start);
	user_rodata_end = __pa(_user_rodata_end);
	user_data_start = __pa(_user_data_start);
	user_data_end = __pa(_user_data_end);
	user_bss_start = __pa(_user_bss_start);
	user_bss_end = __pa(_user_bss_end);

	assert(is_pointer_aligned(user_text_start, ~(PAGE_SIZE - 1)));
	assert(is_pointer_aligned(user_text_end, ~(PAGE_SIZE - 1)));
	assert(is_pointer_aligned(user_rodata_start, ~(PAGE_SIZE - 1)));
	assert(is_pointer_aligned(user_rodata_end, ~(PAGE_SIZE - 1)));
	assert(is_pointer_aligned(user_data_start, ~(PAGE_SIZE - 1)));
	assert(is_pointer_aligned(user_data_end, ~(PAGE_SIZE - 1)));
	assert(is_pointer_aligned(user_bss_start, ~(PAGE_SIZE - 1)));
	assert(is_pointer_aligned(user_bss_end, ~(PAGE_SIZE - 1)));

	printk("user_text_start=%p\n", user_text_start);
	printk("user_text_end=%p\n", user_text_end);
	printk("user_rodata_start=%p\n", user_rodata_start);
	printk("user_rodata_end=%p\n", user_rodata_end);
	printk("user_data_start=%p\n", user_data_start);
	printk("user_data_end=%p\n", user_data_end);
	printk("user_bss_start=%p\n", user_bss_start);
	printk("user_bss_end=%p\n", user_bss_end);

	ret = setup_vma(current, (unsigned long)user_text_start,
		 (unsigned long)user_text_end, VM_READ | VM_EXEC | VM_SHARED,
		 (unsigned long)lma_user);
	if (ret < 0) {
		printk("setup_vma for user text failed\n");
	}

	ret = setup_vma(current, (unsigned long)user_rodata_start,
		 (unsigned long)user_rodata_end, VM_READ | VM_SHARED,
		 (unsigned long)(lma_user + (user_rodata_start
					     - user_text_start)));
	if (ret < 0) {
		printk("setup_vma for user rodata failed\n");
	}

	ret = setup_vma(current, (unsigned long)user_data_start,
		 (unsigned long)user_data_end, VM_READ | VM_WRITE,
		 (unsigned long)(lma_user + (user_data_start
					     - user_text_start)));
	if (ret < 0) {
		printk("setup_vma for user data failed\n");
	}

	ret = setup_vma(current, (unsigned long)user_bss_start,
		 (unsigned long)user_bss_end, VM_READ | VM_WRITE,
		 (unsigned long)(lma_user + (user_bss_start
					     - user_text_start)));
	if (ret < 0) {
		printk("setup_vma for user bss failed\n");
	}

	ret = setup_vma(current, (unsigned long)user_stack_start,
		 (unsigned long)user_stack_end, VM_READ | VM_WRITE, (unsigned long)(-1));
	if (ret < 0) {
		printk("setup_vma for user stack failed\n");
	}

	ret = setup_vma(current, (unsigned long)__uva(UART_BASE),
		 ((unsigned long)__uva(UART_BASE) + PAGE_SIZE),
		 VM_READ | VM_WRITE | VM_SHARED, (unsigned long)UART_BASE);
	if (ret < 0) {
		printk("setup_vma for user uart failed\n");
	}

	current->mm->start_brk = (unsigned long)user_bss_end;
	current->mm->brk = current->mm->start_brk + PAGE_SIZE;

	setup_user_page_mappings(current);

	write_ttbr0_el1((u64)__pa(current->pg_dir), current->pid);
	printk("Setting userspace TTBR0_EL1=%p\n", read_reg(TTBR0_EL1));
	printk("First instruction at userspace: %x\n", *(uint32_t *)vma_user);

	dump_vmas(current);

	memset(user_bss_start, 0, (user_bss_end - user_bss_start));

	asm volatile (
		      "ldr %0, =init\n\t"
		      : "=r" (user_init_addr)
		     );
	switch_to_user_mode((uint64_t)user_init_addr, (uint64_t)user_stack_end);

	return 0;
}

#ifdef TEST_TIMER
static struct timer test_timer1;

void test_timer1_func(unsigned long p)
{
	printk("%s: p=%p\n", __FUNCTION__, p);
	mod_timer(&test_timer1, (get_tick() + 1 * HZ));
}

static struct timer test_timer2;

void test_timer2_func(unsigned long p)
{
	printk("%s: p=%p\n", __FUNCTION__, p);
	mod_timer(&test_timer2, (get_tick() + 3 * HZ));
}

static void
test_timer(char *p)
{
	printk("%s: p=%p\n", __FUNCTION__, p);
	init_timer(&test_timer1);
	test_timer1.expires = get_tick() + 1 * HZ;
	test_timer1.function = test_timer1_func;
	test_timer1.data = (unsigned long)&test_timer1;
	add_timer(&test_timer1);

	init_timer(&test_timer2);
	test_timer2.expires = get_tick() + 3 * HZ;
	test_timer2.function = test_timer2_func;
	test_timer2.data = (unsigned long)&test_timer2;
	add_timer(&test_timer2);
}
#endif

#ifdef TEST_MUTEX
static int test_mutex_flag;
static mutex_t test_mutex_mutex;

static void test_mutex(int *p)
{
	while (true) {
		mutex_lock(&test_mutex_mutex);

		test_mutex_flag = !test_mutex_flag;
		printk("%s: test_mutex_flag=%d\n", get_current_proc()->comm, test_mutex_flag);
		msleep(10);

		mutex_unlock(&test_mutex_mutex);
		msleep(10);
	}

}
#endif

void main(void)
{
	int ret;
	uint64_t reg;
	uint64_t psci_version;
	int i;

	clear_linear_bss();

	init_printk();
	init_uart();
	init_sched();
	init_timer_module();

#ifdef DEBUG_GIC
	printk("distributor interrupts cpu targets:\n");
	for (i = 0x800; i < 0x908; i += 4) {
		printk("%x, %p\n", i,
		       *((unsigned int *)__va(VIRT_GIC_DIST_BASE+(long)i)));
	}
#endif
	set_int_cpu(VIRT_GIC_DIST_BASE, VIRT_INT_UART, INT_TYPE_LEVEL, 0x1);
	set_int_cpu(VIRT_GIC_DIST_BASE, VIRT_INT_RTC, INT_TYPE_LEVEL, 0x2);

	set_int_cpu(VIRT_GIC_DIST_BASE, VIRT_INT_TIMER, INT_TYPE_EDGE,
		    0 /* unused for PPI */ );

	enable_dist_int_send(VIRT_GIC_DIST_BASE);

	config_gic_cpu();
	enable_irq();

	config_rtc();
	config_hw_timer();

	printk("Compile info: " __DATE__ " " __TIME__ "\n");
	{
		static int bss_is_zero;
		printk("Check linear bss is cleared: bss_is_zero=%d\n", bss_is_zero);
	}

	printk("main=%p\n", main);
	printk("%d\n", add(16, 84));

	printk("VA_START=%p\n", VA_START);
	printk("PAGE_OFFSET=%p\n", PAGE_OFFSET);

	printk("CPU core id=%d\n", get_cpu_core_id());

	reg = read_reg(MAIR_EL1);
	printk("MAIR_EL1=%p\n", ((void *)reg));

	printk("TTBR0_EL1=%p\n", ((void *)reg));

	reg = read_reg(TTBR0_EL1);
	printk("TTBR0_EL1=%p\n", ((void *)reg));

	reg = read_reg(TTBR1_EL1);
	printk("TTBR1_EL1=%p\n", ((void *)reg));

	reg = read_reg(TCR_EL1);
	printk("TCR_EL1=%p\n", ((void *)reg));

	reg = read_reg(SCTLR_EL1);
	printk("SCTLR_EL1=%p\n", ((void *)reg));

	reg = read_reg(ID_AA64MMFR0_EL1);
	printk("ID_AA64MMFR0_EL1=%p\n", ((void *)reg));

	reg = read_reg(CurrentEL);
	printk("current EL=%d\n", ((int32_t)reg>>2));

	reg = read_reg(vbar_el1);
	printk("vbar_el1=%p\n", ((void *)reg));

	reg = read_reg(CTR_EL0);
	printk("CTR_EL0=%p\n", ((void *)reg));
	printk("CTR_EL0.ERG=%d\n", ((reg >> 20) & 0xF));

	reg = read_reg(spsel);
	printk("spsel=%d\n", ((void *)reg));
	printk("spsel addr=%p\n", ((uint64_t)&reg-PAGE_OFFSET));
	printk("spsel@linear_addr=%d\n",
	       (*(uint64_t *)((uint64_t)&reg-PAGE_OFFSET)));
	assert(reg == (*(uint64_t *)((uint64_t)&reg-PAGE_OFFSET)));

	*(int *)0 = 1234;
	ret = *(int *)0;
	printk("ret=%d\n", ret);

	ret = 0;
	ret = 1/ret;
	printk("ret=%d\n", ret);

	/* Trigger an El1 sync exception */
	asm volatile (
		"svc #0x1234\n\t"
	     );

	test_printk();

	psci_version = __invoke_psci_fn_hvc(PSCI_0_2_FN_PSCI_VERSION, 0, 0, 0);
	printk("psci_version=%d\n", psci_version);
	/* The psci spec version is a small integer. */
	assert(psci_version < 0xFF);

	reg = read_reg(MPIDR_EL1);
	printk("MPIDR_EL1=%p\n", ((void *)reg));

	for (i = 1; i < NUM_CPUS; i++) {
		ret = __invoke_psci_fn_hvc(PSCI_0_2_FN64_CPU_ON, i, (unsigned long)KERNEL_START, 0);
		printk("__invoke_psci_fn_hvc ret=%d\n", ret);
	}

	write_sys_reg(TTBR0_EL1, 0);	/* Remove identity mapping */
	invalidate_tlb();

	init_kmalloc_free();

	ret = kernel_thread("init", kernel_init, NULL);
	if (ret < 0) {
		return;
	}

	ret = kernel_thread("init2", kernel_init, (void *)2);
	if (ret < 0) {
		return;
	}

	ret = kernel_thread("exit", kernel_exit, NULL);
	if (ret < 0) {
		return;
	}

	ret = kernel_thread("test_exception", test_exception, NULL);
	if (ret < 0) {
		return;
	}

#ifdef TEST_KMALLOC_GET_PAGES
	ret = kernel_thread("test_kmalloc", test_kmalloc, NULL);
	if (ret < 0) {
		return;
	}

	ret = kernel_thread("test_kmalloc2", test_kmalloc, NULL);
	if (ret < 0) {
		return;
	}

	ret = kernel_thread("test_get_pages", test_get_free_pages, NULL);
	if (ret < 0) {
		return;
	}

#endif
	ret = kernel_thread("user_test", user_test, NULL);
	if (ret < 0) {
		return;
	}

#ifdef TEST_TIMER
	ret = kernel_thread("test_timer", (void *)test_timer, NULL);
	if (ret < 0) {
		return;
	}
#endif
#ifdef TEST_MUTEX
	init_mutex(&test_mutex_mutex);
	ret = kernel_thread("test_mutex_a_proc", (void *)test_mutex, NULL);
	if (ret < 0) {
		return;
	}

	ret = kernel_thread("test_mutex_b_proc", (void *)test_mutex, NULL);
	if (ret < 0) {
		return;
	}
#endif

	dump_tasks();
	while (true) {
		cpu_idle();
	}
}

void main_secondary_cpu(void)
{
	uint64_t reg;

	write_sys_reg(TTBR0_EL1, 0);	/* Remove identity mapping */
	invalidate_tlb();

	printk("spsel addr=%p\n", ((uint64_t)&reg-PAGE_OFFSET));

	printk("CPU core id=%d\n", get_cpu_core_id());

	reg = read_reg(MPIDR_EL1);
	printk("MPIDR_EL1=%p\n", ((void *)reg));

	set_int_cpu(VIRT_GIC_DIST_BASE, VIRT_INT_TIMER, INT_TYPE_EDGE,
		    0 /* unused for PPI */ );

	config_gic_cpu();
	enable_irq();

	config_hw_timer();

	while (true) {
		cpu_idle();
	}
}
