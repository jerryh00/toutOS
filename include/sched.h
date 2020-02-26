#ifndef __SCHED_H__
#define __SCHED_H__

#include <mm_types.h>

#define USER_STACK_START 0x20000000
#define USER_STACK_SIZE 0x800000

#define MAX_BRK_ADDR 0x0C000000

struct cpu_context {
	unsigned long x19;
	unsigned long x20;
	unsigned long x21;
	unsigned long x22;
	unsigned long x23;
	unsigned long x24;
	unsigned long x25;
	unsigned long x26;
	unsigned long x27;
	unsigned long x28;
	unsigned long fp;
	unsigned long sp;
	unsigned long pc;
};

struct thread_struct {
	struct cpu_context cpu_context;
};

#define TASK_COMM_LEN 16

struct task_struct {
	volatile long state;
	void *stack;
	struct thread_struct thread;
	int pid;
	int in_use;
	char comm[TASK_COMM_LEN];
	uint64_t *pg_dir;
	struct mm_struct *mm;
	u64 stime;
	u64 utime;
};

struct thread_info {
	struct task_struct *task;
};

extern struct task_struct *__switch_to(struct task_struct *,
				       struct task_struct *);

#define switch_to(prev, next, last)					\
	do {								\
		((last) = __switch_to((prev), (next)));			\
	} while (0)

#define KERNEL_STACK_SIZE 4096

static inline struct thread_info *current_thread_info(void)
{
	unsigned long sp;
	struct thread_info *ti;

	asm volatile (
		      "mov %0, SP\n\t"
		      : "=r"(sp)
		     );

	ti = (struct thread_info *)(sp & ~(KERNEL_STACK_SIZE - 1));

	return ti;
}

void schedule(void);

int schedule_timeout(unsigned int timeout);

void msleep(unsigned int msecs);

int kernel_thread(const char *name, const void *fn, const void *args);

#define MAX_NUM_PROCESSES 32

enum process_state {INITIALIZING, RUNNING, SLEEPING, STOPPED, NUM_PROC_STATES};

void dump_tasks(void);

void switch_to_user_mode(uint64_t user_pc, uint64_t user_sp);

struct task_struct *get_current_proc(void);

void do_exit(unsigned long ret_val);

int setup_vma(struct task_struct *t, unsigned long vm_start,
	      unsigned long vm_end, unsigned long vm_flags,
	      unsigned long lma);

struct vm_area_struct *find_vma(const struct mm_struct *mm, unsigned long addr);

void dump_vma(struct vm_area_struct *vma);
void dump_vmas(struct task_struct *t);

int add_pages_block(struct vm_area_struct *vma, void *user_virt_addr,
		    void *linear_addr, unsigned int order);

void setup_user_page_mapping(uint64_t *pg_dir, void *virt_addr,
				    void *phy_addr, size_t size,
				    int rdonly);
void setup_user_page_mappings(struct task_struct *t);

int unmap_user_page_mapping(int asid, uint64_t *pg_dir, void *virt_addr, size_t size);

struct task_struct *get_task_slot(void);

void free_task_slot(struct task_struct *t);

void exit_mm(struct task_struct *tsk);

void init_sched(void);

struct task_struct *pid_to_task(int pid);

void set_task_state(struct task_struct *t, enum process_state state);

#endif
