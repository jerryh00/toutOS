#include <arch.h>
#include <sched.h>
#include <hardware.h>
#include <string.h>
#include <printk.h>
#include <mmu.h>
#include <memory.h>
#include <spinlock.h>
#include <percpu.h>
#include <timer.h>
#include <hw_timer.h>

#include "../mm/page_table.c"
void *dummy_sched_c = walk_virt_addr;

extern struct task_struct *cpu_switch_to(struct task_struct *prev,
					 struct task_struct *next);

struct task_struct *__switch_to(struct task_struct *prev,
				struct task_struct *next)
{
	struct task_struct *last;

	last = cpu_switch_to(prev, next);

	return last;
}

extern uint64_t swapper_stacks[NUM_CPUS][PAGE_SIZE/8];

struct task_struct swapper_task_struct[NUM_CPUS] = {
	{
	       .state = RUNNING,
	       .stack = __va(&swapper_stacks[0][0]),       /* for cpu0 */
	       .pid = 0,
	       .in_use = true,
	       .comm = "swapper",
	}
};

static uint64_t proc_kernel_stacks[MAX_NUM_PROCESSES][KERNEL_STACK_SIZE/8] __attribute__ \
		 ((aligned (KERNEL_STACK_SIZE)));

static struct spinlock tasks_lock;
static struct task_struct tasks[MAX_NUM_PROCESSES];

void init_sched(void)
{
	int i;

	for (i = 0; i < NUM_CPUS; i++) {
		swapper_task_struct[i].state = RUNNING;
		swapper_task_struct[i].stack = swapper_task_struct[0].stack + i * PAGE_SIZE;
		*(uint64_t *)swapper_task_struct[i].stack = (uint64_t)&swapper_task_struct[i];
		swapper_task_struct[i].pid = 0;
		swapper_task_struct[i].in_use = true;
		strncpy(swapper_task_struct[i].comm, "swapper", TASK_COMM_LEN-1);
		swapper_task_struct[i].comm[TASK_COMM_LEN-1] = '\0';
	}

	spin_lock_init(&tasks_lock);
}

struct task_struct *get_current_proc(void)
{
	struct thread_info *ti = current_thread_info();

	return ti->task;
}

static int is_task_ready(const struct task_struct *t)
{
	return (t->in_use == true && t->state == RUNNING);
}

static struct task_struct *get_next_proc(void)
{
	unsigned long flags;
	int core_id = get_cpu_core_id();
	u64 min_time;
	int min_time_index = -1;

	flags = spin_lock_irqsave(&tasks_lock);

	for (int i = core_id; i < MAX_NUM_PROCESSES; i += NUM_CPUS) {
		if (is_task_ready(&tasks[i])) {
			u64 proc_time = tasks[i].stime + tasks[i].utime;
			if (min_time_index == -1 || min_time > proc_time) {
				min_time_index = i;
				min_time = proc_time;
			}
		}
	}

	spin_unlock_irqrestore(&tasks_lock, flags);
	if (min_time_index >=0) {
		return &tasks[min_time_index];
	} else {
		return NULL;
	}
}

void schedule(void)
{
#ifdef DEBUG_SCHED
	unsigned long sp;
#endif

	struct task_struct *prev;
	struct task_struct *next;
#ifdef DEBUG_SCHED
	printk("In schedule\n");
#endif
	prev = get_current_proc();
	next = get_next_proc();

	if (next == NULL) {
		next = &swapper_task_struct[get_cpu_core_id()];
	}
#ifdef DEBUG_SCHED
	asm volatile (
		      "mov %0, SP\n\t"
		      : "=r"(sp)
		     );

	printk("prev=%p, next=%p\n", prev, next);
	printk("next->comm=%s, prev sp=%p\n", next->comm, sp);
#endif
	if (prev != next) {
#ifdef DEBUG_SCHED
		printk("Switching from %s(pid=%d) to %s(pid=%d)\n",
		       prev->comm,  prev->pid, next->comm, next->pid);
		printk("next->pg_dir=%p\n", next->pg_dir);
		printk("__pa(next->pg_dir)=%p\n", __pa(next->pg_dir));
#endif
		if (next->pg_dir != NULL) {
			write_ttbr0_el1((u64)__pa(next->pg_dir), next->pid);
		}
		switch_to(prev, next, prev);
#ifdef DEBUG_SCHED
		printk("Last %s(pid=%d)\n", prev->comm,  prev->pid);
#endif
	}
}

static void process_timeout(unsigned long __data)
{
	struct task_struct *t = (struct task_struct *)__data;

	set_task_state(t, RUNNING);
}

int schedule_timeout(unsigned int timeout)
{
	struct timer timer;
	unsigned int expires;
	int delta;

	expires = timeout + get_tick();
	init_timer(&timer);
	timer.expires = expires;
	timer.function = process_timeout;
	timer.data = (unsigned long)get_current_proc();
	add_timer(&timer);
	schedule();
	del_timer(&timer);

	delta = expires - get_tick();
	if (delta < 0) {
		return 0;
	} else {
		return delta;
	}
}

void msleep(unsigned int msecs)
{
	int timeout = ((msecs + TICK - 1) / TICK);

	while (timeout != 0) {
		set_task_state(get_current_proc(), SLEEPING);
		timeout = schedule_timeout(timeout);
	}
}

void free_task_slot(struct task_struct *t)
{
	unsigned long flags;

	if (t == NULL) {
		printk("%s: task_struct is null\n", __FUNCTION__);
		return;
	}

	flags = spin_lock_irqsave(&tasks_lock);
	t->in_use = false;
	memset(t, 0, sizeof (*t));
	spin_unlock_irqrestore(&tasks_lock, flags);
}

struct task_struct *get_task_slot(void)
{
	int i;
	unsigned long flags;

	flags = spin_lock_irqsave(&tasks_lock);
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (tasks[i].in_use == false) {
			tasks[i].in_use = true;
			tasks[i].pid = i + 1;
			tasks[i].stack = &proc_kernel_stacks[i];
			((struct thread_info *)(tasks[i].stack))->task =
				&tasks[i];

			spin_unlock_irqrestore(&tasks_lock, flags);
			return &tasks[i];
		} else if (tasks[i].state == STOPPED) {
			printk("freeing task slot, pid=%d\n", tasks[i].pid);
			invalidate_tlb_by_asid(tasks[i].pid);
			tasks[i].in_use = false;
			memset(&tasks[i], 0, sizeof (tasks[i]));
		}
	}

	spin_unlock_irqrestore(&tasks_lock, flags);
	return NULL;
}

struct task_struct *pid_to_task(int pid)
{
	int index = pid - 1;

	if (!(index >=0 && index < MAX_NUM_PROCESSES)) {
		printk("%s: index out of bound, index=%d\n", __FUNCTION__, index);
		return NULL;
	}

	return &tasks[index];
}

void set_task_state(struct task_struct *t, enum process_state state)
{
	unsigned long flags;

	if (t == NULL) {
		printk("%s: task_struct is null\n", __FUNCTION__);
		return;
	}
	if (state < 0 || state >= NUM_PROC_STATES) {
		printk("%s: task state is invalid, state=%d\n", __FUNCTION__, state);
		return;
	}

	flags = spin_lock_irqsave(&tasks_lock);
	t->state = state;
	spin_unlock_irqrestore(&tasks_lock, flags);
}

extern void call_thread_func(void);

int kernel_thread(const char *name, const void *fn, const void *args)
{
	struct task_struct *t;
	unsigned long *sp;
	struct mm_struct *mm;

	if (name == NULL) {
		printk("%s: name is null\n", __FUNCTION__);
		return -1;
	}
	if (fn == NULL) {
		printk("%s: function is null\n", __FUNCTION__);
		return -1;
	}

	t = get_task_slot();
	if (t == NULL) {
		printk("%s: get_task_slot failed for %s\n", __FUNCTION__, name);
		return -1;
	}

	t->thread.cpu_context.fp = 0;	/* ? */
	t->thread.cpu_context.pc = (unsigned long)call_thread_func;
	strncpy(t->comm, name, (TASK_COMM_LEN - 1));

	mm = kzalloc(sizeof (*mm));
	if (mm == NULL) {
		free_task_slot(t);
		printk("%s: memory allocation failed for %s\n", __FUNCTION__, name);
		return -1;
	}
	t->mm = mm;

	/* Save the fn and args on stack. */
	sp = (unsigned long *)(((char *)(t->stack)) + KERNEL_STACK_SIZE);
	*(--sp) = (unsigned long)fn;
	*(--sp) = (unsigned long)args;
	t->thread.cpu_context.sp = (unsigned long)sp;

	t->state = RUNNING;

	return 0;
}

#define NUM_ENTRY_PER_PAGE (PAGE_SIZE/sizeof(void *))

static void free_page_tables_layer1(uint64_t *page_table)
{
	int i;

	if (page_table == NULL) {
		return;
	}

	for (i = 0; i < NUM_ENTRY_PER_PAGE; i++) {
		if (page_table[i] != 0) {
			void *entry = __va(get_phy_addr(page_table[i]));
#ifdef DEBUG_EXIT_MM
			printk("Freeing layer1 page table entry %p\n", entry);
#endif
			free_pages(entry, 0);
		}
	}

#ifdef DEBUG_EXIT_MM
	printk("Freeing layer1 page table %p\n", page_table);
#endif
	free_pages(page_table, 0);
}

static void free_page_tables(uint64_t *pg_dir)
{
	int i;

	if (pg_dir == NULL) {
		return;
	}

	for (i = 0; i < NUM_ENTRY_PER_PAGE; i++) {
		if (pg_dir[i] != 0) {
			free_page_tables_layer1(__va(get_phy_addr(pg_dir[i])));
		}
	}

#ifdef DEBUG_EXIT_MM
	printk("Freeing page table directory %p\n", pg_dir);
#endif
	free_pages(pg_dir, 0);
}

static void free_pages_blocks(const struct vm_area_struct *vma)
{
	struct pages_block *pb;

	if (vma == NULL) {
		return;
	}

	list_for_each_entry(pb, &vma->pages_block_list, list) {
#ifdef DEBUG_EXIT_MM
		printk("%s: user_virt_addr=%p, linear_addr=%p, order=%d\n", __FUNCTION__, pb->user_virt_addr, pb->linear_addr, pb->order);
#endif
		free_pages(pb->linear_addr, pb->order);
	}
}

void exit_mm(struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	struct vm_area_struct *vma_dummy;

	if (tsk == NULL) {
		printk("%s: task is null\n", __FUNCTION__);
		return;
	}
	if (tsk->mm == NULL) {
		printk("%s: task->mm is null\n", __FUNCTION__);
		return;
	}

	for (vma = tsk->mm->mmap; vma != NULL; vma = vma_dummy) {
		vma_dummy = vma->vm_next;
		if (!(vma->vm_flags & VM_SHARED)) {
			free_pages_blocks(vma);
		}
#ifdef DEBUG_EXIT_MM
		printk("Freeing vma %p\n", vma);
#endif
		kfree(vma);
	}

	kfree(tsk->mm);
	free_page_tables(tsk->pg_dir);
}

void do_exit(unsigned long ret_val)
{
	struct task_struct *t;

	t = get_current_proc();
	printk("%s: %s exited, pid=%d, ret_val=%d\n", __FUNCTION__, t->comm, t->pid, ret_val);

	exit_mm(t);
	t->state = STOPPED;
	schedule();
}

static void dump_task_info(struct task_struct *t)
{
	struct thread_info *ti;

	ti = (struct thread_info *)(t->stack);

#ifdef DEBUG_SCHED
	if (t->in_use == true) {
		u64 *tmp = (u64 *)ti;
		printk("t=%p, ti->task=%p, stack=%p\n", t, ti->task, t->stack);
		printk("%p %p %p %p \n", tmp[-7], tmp[-6], tmp[-5], tmp[-4]);
		printk("%p %p %p %p \n", tmp[-3], tmp[-2], tmp[-1], tmp[0]);
		printk("%p %p %p %p \n", tmp[1], tmp[2], tmp[3], tmp[4]);
		printk("%p %p %p %p \n", tmp[5], tmp[6], tmp[7], tmp[8]);
	}
#endif
	if (t->in_use == true && ti->task != t) {
		printk("Error: ti->task(%p) != t(%p)\n", ti->task, t);
		return;
	}
	printk("@%p: comm=%s, state=%d, stack=%p, pid=%d, in_use=%d, pg_dir=%p\n",
	       t, t->comm, t->state, t->stack, t->pid, t->in_use, t->pg_dir);
	if (t->mm != NULL) {
		printk("@%p: mm=%p, mm->mmap=%p, mm->start_brk=%p, mm->brk=%p\n",
		       t, t->mm, t->mm->mmap, t->mm->start_brk, t->mm->brk);
	}
	printk("@%p: stime=%d, utime=%d\n", t, t->stime, t->utime);
}

void dump_tasks(void)
{
	int i;
	unsigned long flags;

	printk("Dumping tasks info\n");

	flags = spin_lock_irqsave(&tasks_lock);
	for (i = 0; i < NUM_CPUS; i++) {
		dump_task_info(&swapper_task_struct[i]);
	}

	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		dump_task_info(&tasks[i]);
	}
	spin_unlock_irqrestore(&tasks_lock, flags);
}

int setup_vma(struct task_struct *t, unsigned long vm_start,
	      unsigned long vm_end, unsigned long vm_flags,
	      unsigned long lma)
{
	struct vm_area_struct *vma;

	if (t == NULL) {
		printk("%s: task is null\n", __FUNCTION__);
		return -1;
	}

	vma = kzalloc(sizeof (*vma));
	if (vma == NULL) {
		return -1;
	}
#ifdef DEBUG_VMA
	printk("Newly allocated vma=%p\n", vma);
	printk("vm_start=%p, vm_end=%p, vm_flags=%p, lma=%p\n",
	       vm_start, vm_end, vm_flags, lma);
#endif

	vma->vm_start = vm_start;
	vma->vm_end = vm_end;
	vma->vm_flags = vm_flags;
	vma->lma = lma;
	INIT_LIST_HEAD(&vma->pages_block_list);

	if (t->mm->mmap != NULL) {
		t->mm->mmap->vm_prev = vma;
	}
	vma->vm_next = t->mm->mmap;
	vma->vm_prev = NULL;
	t->mm->mmap = vma;

	return 0;
}

void dump_vma(struct vm_area_struct *vma)
{
	if (vma == NULL) {
		printk("%s: vma is null\n", __FUNCTION__);
		return;
	}

	printk("Dumping vma@%p\n", vma);
	printk("vm_start=%p\n", vma->vm_start);
	printk("vm_end=%p\n", vma->vm_end);
	printk("vm_flags=%p\n", vma->vm_flags);
	printk("lma=%p\n", vma->lma);
	printk("vm_next=%p\n", vma->vm_next);
	printk("vm_prev=%p\n", vma->vm_prev);
}

struct vm_area_struct *find_vma(const struct mm_struct *mm, unsigned long addr)
{
	struct vm_area_struct *vma;

	if (mm == NULL) {
		printk("%s: mm is null\n", __FUNCTION__);
		return NULL;
	}

	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		if (addr >= vma->vm_start && addr < vma->vm_end) {
#ifdef DEBUG_VMA
			printk("Found vma\n");
			dump_vma(vma);
#endif
			return vma;
		}
	}

	return NULL;
}

void dump_vmas(struct task_struct *t)
{
	struct vm_area_struct *vma;

	if (t == NULL) {
		printk("%s: task is null\n", __FUNCTION__);
		return;
	}

	for (vma = t->mm->mmap; vma != NULL; vma = vma->vm_next) {
		dump_vma(vma);
	}
}

int add_pages_block(struct vm_area_struct *vma, void *user_virt_addr,
		    void *linear_addr, unsigned int order)
{
	struct pages_block *pb;

	if (vma == NULL || user_virt_addr == NULL || linear_addr == NULL) {
		return -1;
	}

	pb = (struct pages_block *)kmalloc(sizeof(*pb));
	if (pb == NULL) {
		return -1;
	}
	pb->user_virt_addr = user_virt_addr;
	pb->linear_addr = linear_addr;
	pb->order = order;
	list_add_tail(&pb->list, &vma->pages_block_list);

	return 0;
}

void setup_user_page_mapping(uint64_t *pg_dir, void *virt_addr,
				    void *phy_addr, size_t size,
				    int rdonly)
{
	struct memory_map map = {
		/* RAM */
		.phy_addr = (uint64_t)phy_addr,
		.virt_addr = (uint64_t)virt_addr,
		.size = size,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_USER |
			PTE_BLOCK_INNER_SHARE | PTE_BLOCK_NG,
	};
	if (pg_dir == NULL) {
		printk("%s: pg_dir is null\n", __FUNCTION__);
		return;
	}
	if (phy_addr == (void *)(-1)) {
		return;
	}
	if (rdonly) {
		map.attrs |= PTE_RDONLY;
	}

	add_single_map(&map, pg_dir, get_zeroed_pages, true);

#ifdef SETUP_USER_PAGE_MAPPING
	printk("actual user lma:%p\n", walk_virt_addr(pg_dir, virt_addr, true));
#endif
}

void setup_user_page_mappings(struct task_struct *t)
{
	struct vm_area_struct *vma;
	int rdonly;

	if (t == NULL) {
		printk("%s: task is null\n", __FUNCTION__);
		return;
	}

	for (vma = t->mm->mmap; vma != NULL; vma = vma->vm_next) {
		rdonly = !(vma->vm_flags & VM_WRITE);
		setup_user_page_mapping(t->pg_dir, (void *)vma->vm_start,
					(void *)vma->lma,
					(vma->vm_end - vma->vm_start),
					rdonly);
	}
}

int unmap_user_page_mapping(int asid, uint64_t *pg_dir, void *virt_addr, size_t size)
{
	void *addr;
	void *addr_end;
	int ret = 0;

#ifdef DEBUG_PAGE_TABLE
	printk("%s enter, virt_addr=%p, size=%p\n", __FUNCTION__, virt_addr, size);
#endif
	if (pg_dir == NULL) {
		printk("%s: pg_dir is null\n", __FUNCTION__);
		return -1;
	}
	if (virt_addr == NULL || !is_pointer_aligned(size, ~(PAGE_SIZE - 1))) {
		return -1;
	}

	addr_end = virt_addr + size;
	for (addr = virt_addr; addr < addr_end; addr += PAGE_SIZE) {
		ret = unmap_one_user_page(asid, pg_dir, addr, free_pages);
		if (ret < 0) {
			printk("unmap_one_user_page failed, addr=%p\n", addr);
			return -1;
		}
	}
#ifdef DEBUG_PAGE_TABLE
	printk("%s exit\n", __FUNCTION__);
#endif

	return ret;
}
