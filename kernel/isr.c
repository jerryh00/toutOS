#include <arch.h>
#include <hardware.h>
#include <printk.h>
#include <uart.h>
#include <circular_buffer.h>
#include <hw_timer.h>
#include <misc.h>
#include <percpu.h>
#include <sched.h>
#include <mmu.h>
#include <stddef.h>
#include <memory.h>
#include <string.h>
#include <softirq.h>
#include <time.h>
#include <wait.h>

DEFINE_PER_CPU(uint64_t[MAX_NUM_INTERRUPTS], irq_trigger_count);

extern int uart_busy;
extern struct concurrent_cbuf kernel_log;

extern void child_returns_from_fork(void);

static void handle_rtc_irq(void)
{
	*(uint32_t *)__va(VIRT_RTC_RTCICR) = 1;
}

isr_func_t isr_func[MAX_NUM_INTERRUPTS] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, handle_timer_irq, NULL,
	NULL, handle_uart_irq, handle_rtc_irq, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

void do_unknown_interrupt(void)
{
	printk("core %d: Unknown interrupt.\n", get_cpu_core_id());
	while (1);
}

static void show_pt_regs(const struct pt_regs *regs)
{
	int i;

	for (i = 0; i < 31; i++) {
		printk("X%d=%p\n", i, ((void *)regs->regs[i]));
	}
	printk("sp=%p\n", ((void *)regs->sp));
	printk("pc=%p\n", ((void *)regs->pc));
	printk("pstate=%p\n", ((void *)regs->pstate));
}

static uint64_t get_ec_from_esr(uint64_t esr)
{
	/* Exception class */
	return ((esr >> 26) & 0x3F);
}

static int is_kernel_inst_abort(uint64_t esr)
{
	return (get_ec_from_esr(esr) == 0x21);
}

static int is_kernel_data_abort(uint64_t esr)
{
	return (get_ec_from_esr(esr) == 0x25);
}

static int is_svc(uint64_t esr)
{
	return (get_ec_from_esr(esr) == 0x15);
}

static void show_esr(uint64_t esr)
{
	printk("esr=%p\n", (void *)esr);
	printk("Exception Class: 0X%x\n", get_ec_from_esr(esr));
	if ((esr >> 25) & 0x1) {
		printk("Instruction length: 32-bit trapped instruction\n");
	} else {
		printk("Instruction length: 16-bit trapped instruction\n");
	}
	printk("Instruction specific syndrome: 0X%x\n", (esr & 0xFFFFFF));
}

void do_el1_sync(uint64_t addr, uint64_t esr, struct pt_regs *regs)
{
	printk("cpu%d El1 sync exception.\n", get_cpu_core_id());
	printk("addr=%p\n", (void *)addr);
	show_esr(esr);
	show_pt_regs(regs);
	if (is_kernel_inst_abort(esr) || is_kernel_data_abort(esr)) {
		panic("Kernel panic, invalid address %p\n", addr);
	}
}

void irq_handler(struct pt_regs *regs)
{
	uint32_t reg_GICC_IAR;
	uint32_t irq;
	int core_id;

	reg_GICC_IAR = *(uint32_t *)__va(VIRT_GIC_CPU_BASE + 0x0C);
	irq = reg_GICC_IAR;

	core_id = get_cpu_core_id();

	if (irq == IRQ_SPURIOUS) {
		printk("cpu%d: spurious interrupt\n", core_id);
		return;
	}

	if (irq < MAX_NUM_INTERRUPTS) {
		if (isr_func[irq] != NULL) {
			per_cpu(irq_trigger_count, core_id)[irq]++;
			isr_func[irq]();
		} else {
			printk("Null handler for irq %d\n", irq);
		}
	} else {
		printk("cpu%d: irq out of range: %d\n", core_id, irq);
	}
	*(uint32_t *)__va(VIRT_GIC_CPU_BASE + 0x10) = irq;

	if (irq == IRQ_TIMER) {
		struct task_struct *current = get_current_proc();
		int in_user_mode = user_mode(regs);
		if (in_user_mode) {
			current->utime += 1;
		} else {
			current->stime += 1;
		}
		if (in_user_mode || current->pid == 0) {
			enable_irq();
#ifdef DEBUG_SCHED
			printk("Showing pt_regs before schedule\n");
			show_pt_regs(regs);
#endif
			schedule();
#ifdef DEBUG_SCHED
			printk("Showing pt_regs after schedule\n");
			show_pt_regs(regs);
#endif
			disable_irq();
		}
	}
	do_softirq();
}

static void sys_fork(struct pt_regs *regs)
{
	struct task_struct *parent_task = get_current_proc();
	struct task_struct *child_task;
	uint64_t *pg_dir_user_map;
	struct mm_struct *mm;

#ifdef DEBUG_FORK
	printk("In sys_fork\n");
#endif

	child_task = get_task_slot();
	if (child_task == NULL) {
		goto fail_child_task;
	}

	memcpy(child_task->stack, parent_task->stack, PAGE_SIZE);
	((struct thread_info *)(child_task->stack))->task = child_task;

	memcpy(&child_task->thread, &parent_task->thread,
	       sizeof (struct thread_struct));

	strncpy(child_task->comm, parent_task->comm, (TASK_COMM_LEN - 1));

	mm = kzalloc(sizeof (*mm));
	if (mm == NULL) {
		printk("Memory allocation for mm_struct failed\n");
		goto fail_mm_alloc;
	}
	child_task->mm = mm;

	pg_dir_user_map = get_zeroed_pages(0);
	if (pg_dir_user_map == NULL) {
		printk("Memory allocation for user page directory failed\n");
		goto fail_pg_dir_user_map;
	}
	child_task->pg_dir = pg_dir_user_map;
#ifdef DEBUG_FORK
	printk("child_task->pg_dir=%p\n", child_task->pg_dir);
#endif

	for (struct vm_area_struct *vma = parent_task->mm->mmap; vma != NULL; vma = vma->vm_next) {
		uint64_t *page;
		struct vm_area_struct *child_vma;
		unsigned int order;
		int ret;

		if (vma->vm_flags & VM_SHARED) {
			ret = setup_vma(child_task, vma->vm_start, vma->vm_end,
				  vma->vm_flags, vma->lma);
			if (ret < 0) {
				printk("setup_vma failed\n");
				goto fail_setup_vma;
			}
		} else if (vma->lma != (unsigned long)(-1)) {
			order = get_order(vma->vm_end - vma->vm_start);
			page = get_free_pages(order);
			if (page == NULL) {
				printk("sys_fork get_free_pages failed\n");
				goto fail_setup_vma;
			}
			ret = setup_vma(child_task, vma->vm_start, vma->vm_end,
				  vma->vm_flags, (unsigned long)__pa(page));
			if (ret < 0) {
				free_pages(page, order);
				printk("setup_vma failed\n");
				goto fail_setup_vma;
			}
			child_vma = find_vma(child_task->mm, vma->vm_start);
			ret = add_pages_block(child_vma, (void *)vma->vm_start, page, order);
			if (ret < 0) {
				printk("sys_fork add pages block to vma failed\n");
				free_pages(page, order);
				goto fail_setup_vma;
			}
#ifdef DEBUG_FORK
			printk("Copying parent_vma=%p, vma->vm_start=%p, "
			       "vma->vm_end=%p\n",
			       vma, vma->vm_start, vma->vm_end);
#endif
			memcpy(page, (void *)vma->vm_start,
			       (vma->vm_end - vma->vm_start));
		} else {
			struct pages_block *pb;
			ret = setup_vma(child_task, vma->vm_start, vma->vm_end,
				  vma->vm_flags, (unsigned long)(-1));
			if (ret < 0) {
				printk("setup_vma failed\n");
				goto fail_setup_vma;
			}
			child_vma = find_vma(child_task->mm, vma->vm_start);
#ifdef DEBUG_FORK
			printk("Copying parent_vma=%p\n", vma);
#endif
			list_for_each_entry(pb, &vma->pages_block_list, list) {
				order = 0;
				page = get_free_pages(order);
				if (page == NULL) {
					printk("sys_fork get_free_pages failed\n");
					goto fail_setup_vma;
				}
				ret = add_pages_block(child_vma, pb->user_virt_addr, page, order);
				if (ret < 0) {
					printk("sys_fork add pages block to vma failed\n");
					free_pages(page, order);
					goto fail_setup_vma;
				}
				memcpy(page, pb->linear_addr,
				       (PAGE_SIZE * (1 << order)));
			}
		}
	}
#ifdef DEBUG_FORK
	dump_vmas(child_task);
#endif
	setup_user_page_mappings(child_task);

	child_task->mm->start_brk = parent_task->mm->start_brk;
	child_task->mm->brk = parent_task->mm->brk;

	child_task->thread.cpu_context.pc = (u64)child_returns_from_fork;
	child_task->thread.cpu_context.sp = (u64)child_task->stack
		+ (u64)IN_PAGE_OFFSET(regs);
	((struct pt_regs *)(child_task->thread.cpu_context.sp))->regs[0] = 0;

	child_task->state = RUNNING;

	regs->regs[0] = child_task->pid;
#ifdef DEBUG_FORK
	printk("regs->regs[0]=%d\n", regs->regs[0]);
#endif

	return;

fail_setup_vma:
fail_pg_dir_user_map:
	exit_mm(child_task);
fail_mm_alloc:
fail_child_task:
	free_task_slot(child_task);

	regs->regs[0] = -1;
}

static void sys_brk(struct pt_regs *regs)
{
	unsigned long addr = regs->regs[0];
	struct task_struct *current = get_current_proc();
	unsigned long oldbrk;
	unsigned long newbrk;
	struct vm_area_struct *vma;
	int ret_val;

	if (addr == 0) {
		goto out;
	}

	if (addr < current->mm->start_brk || addr > MAX_BRK_ADDR) {
		goto out;
	}

	newbrk = UPPER_PAGE_ADDR(addr);
	oldbrk = UPPER_PAGE_ADDR(current->mm->brk);

#ifdef DEBUG_BRK
	printk("%s: newbrk=%p, oldbrk=%p\n", __FUNCTION__, newbrk, oldbrk);
#endif
	if (newbrk < oldbrk) {
		struct pages_block *pb;
		struct pages_block *pb_dummy;

		ret_val = unmap_user_page_mapping(current->pid, current->pg_dir,
						  (void *)newbrk,
						  (oldbrk - newbrk));
		if (ret_val < 0) {
			printk("unmap_user_page_mapping failed\n");
		}
		vma = find_vma(current->mm, current->mm->start_brk);
		list_for_each_entry_safe(pb, pb_dummy, &vma->pages_block_list, list) {
			if (pb->user_virt_addr >= (void *)newbrk &&
			    pb->user_virt_addr < (void *)oldbrk) {
#ifdef DEBUG_BRK
				printk("%s: deleting pages_block user_virt_addr=%p, linear_addr=%p, order=%d\n",
				       __FUNCTION__, pb->user_virt_addr, pb->linear_addr, pb->order);
#endif
				list_del(&pb->list);
			}
		}
	}
	current->mm->brk = (unsigned int)addr;

out:
	vma = find_vma(current->mm, current->mm->start_brk);
	if (vma == NULL) {
		printk("Creating vma for brk.\n");
		setup_vma(current, current->mm->start_brk, current->mm->brk,
			  VM_READ | VM_WRITE, (unsigned long)(-1));
	} else {
		vma->vm_end = current->mm->brk;
	}
	regs->regs[0] = current->mm->brk;
#ifdef DEBUG_BRK
	printk("current->mm->brk=%p\n", current->mm->brk);
#endif
}

static void sys_exit(struct pt_regs *regs)
{
	unsigned long status = regs->regs[0];

	do_exit(status);
}

static unsigned int timespec_to_jiffies(const struct timespec *p)
{
	unsigned int local_tick;

	local_tick = p->tv_sec * HZ + (p->tv_nsec + TICK * 1000000 - 1) * HZ / 1000000000;

	return local_tick;
}

static void sys_nanosleep(struct pt_regs *regs)
{
	struct timespec* req = (struct timespec *)regs->regs[0];
	struct timespec* rem = (struct timespec *)regs->regs[1];
	int ret;
	int timeout;
	int end_tick;

	timeout = timespec_to_jiffies(req);
	if (timeout <= 0) {
		regs->regs[0] = -1;
		return;
	}
	end_tick = timeout + get_tick();
	ret = -1;

	set_task_state(get_current_proc(), SLEEPING);
	while (ret != 0 && timeout > 0) {
		ret = schedule_timeout(timeout);
		timeout = end_tick - get_tick();
	}

	memset(rem, 0, sizeof (*rem));
	regs->regs[0] = 0;
}

static void sys_pause(struct pt_regs *regs)
{
	set_task_state(get_current_proc(), SLEEPING);
	schedule();
	regs->regs[0] = -1;
}

static ssize_t read_stdin(uint8_t *buf, size_t count)
{
	extern uint8_t uart_in_buf[];
	extern int uib_index;
	extern struct spinlock uib_lock;
	extern struct wait_queue_head uib_wq_head;

	struct wait_queue_entry read_wq_entry;
	struct task_struct *current = get_current_proc();
	int copy_len;
	unsigned long flags;

	flags = spin_lock_irqsave(&uib_lock);
	if (!(uib_index > 0 && uart_in_buf[uib_index-1] == '\n')) {
		init_waitqueue_entry(&read_wq_entry, current);
		add_wait_queue(&uib_wq_head, &read_wq_entry);
		set_task_state(current, SLEEPING);

		while (!(uib_index > 0 && uart_in_buf[uib_index-1] == '\n')) {
			spin_unlock_irqrestore(&uib_lock, flags);
			set_task_state(current, SLEEPING);
			schedule();
			flags = spin_lock_irqsave(&uib_lock);
		}
		set_task_state(current, RUNNING);
		remove_wait_queue(&uib_wq_head, &read_wq_entry);
	}

	if (count < uib_index) {
		printk("%s: count too small, count=%d\n", __FUNCTION__, count);
		spin_unlock_irqrestore(&uib_lock, flags);
		return -1;
	}
	copy_len = uib_index;
	memcpy(buf, uart_in_buf, copy_len);
	uib_index = 0;
	spin_unlock_irqrestore(&uib_lock, flags);

	return copy_len;
}

static void sys_read(struct pt_regs *regs)
{
	int fd = (int)regs->regs[0];
	uint8_t *buf = (uint8_t *)regs->regs[1];
	size_t count = (size_t)regs->regs[2];
	int ret = -1;

	if (fd == 0) {
		ret = read_stdin(buf, count);
	}

	regs->regs[0] = ret;
}

static ssize_t write_stdout(const uint8_t *buf, size_t count)
{
	for (int i = 0; i < count; i++) {
		write_char_uart(buf[i]);
	}

	return count;
}

static void sys_write(struct pt_regs *regs)
{
	int fd = (int)regs->regs[0];
	uint8_t *buf = (uint8_t *)regs->regs[1];
	size_t count = (size_t)regs->regs[2];
	int ret = -1;

	if (fd == 1) {
		ret = write_stdout(buf, count);
	}

	regs->regs[0] = ret;
}

static syscall_func_t syscall_func[MAX_NUM_SYSCALLS] = {
	sys_fork, sys_brk, sys_exit, sys_nanosleep, sys_pause, sys_read, sys_write, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

void do_el0_sync(uint64_t addr, uint64_t esr, struct pt_regs *regs)
{
	uint64_t *page;
	struct task_struct *current = get_current_proc();
	struct vm_area_struct *vma;
	unsigned int order;
	int ret;

	if (is_svc(esr)) {
		regs->orig_x0 = regs->regs[0];
		regs->syscallno = regs->regs[8];

		if (regs->syscallno >= MAX_NUM_SYSCALLS) {
			printk("system call number out of range: %d\n",
			       regs->syscallno);
			regs->regs[0] = -ENOSYS;
			return;
		}

		if (syscall_func[regs->syscallno] == NULL) {
			printk("Null handler for system call %d\n",
			       regs->syscallno);
			regs->regs[0] = -ENOSYS;
			return;
		}

		syscall_func[regs->syscallno](regs);

		return;
	}

	vma = find_vma(current->mm, addr);
	if (vma != NULL) {
		order = 0;
		page = get_free_pages(order);
		if (page == NULL) {
			printk("get_free_pages failed\n");
			return;
		}
		setup_user_page_mapping(current->pg_dir,
					(void *)PAGE_ADDR(addr),
					__pa(page), PAGE_SIZE, false);
		ret = add_pages_block(vma, (void *)PAGE_ADDR(addr), page, order);
		if (ret < 0) {
			printk("add pages block to vma failed\n");
		}

		return;
	}

	printk("cpu%d El0 sync exception.\n", get_cpu_core_id());
	printk("addr=%p\n", (void *)addr);
	show_esr(esr);
	show_pt_regs(regs);
	printk("DAIF=%p\n", read_reg(DAIF));
	printk("TTBR0_EL1=%p\n", read_reg(TTBR0_EL1));

	/* Kill the process if it's EL0 exception for now. */
	do_exit(-1);
}
