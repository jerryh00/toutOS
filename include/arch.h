#ifndef __ARCH_H__
#define __ARCH_H__

/*
 * PSR bits
 */
#define PSR_MODE_EL0t   0x00000000
#define PSR_MODE_MASK   0x0000000f

/* Architecture data model dependent. */
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef int int32_t;
typedef unsigned int uint32_t;

typedef int64_t intptr_t;
typedef uint64_t uintptr_t;

typedef uint64_t u64;
typedef uint32_t u32;

typedef uint64_t size_t;
typedef int64_t ssize_t;

typedef unsigned char uint8_t;

struct pt_regs {
	u64 regs[31];
	u64 sp;
	u64 pc;
	u64 pstate;
	u64 orig_x0;
	u64 syscallno;
};

#define read_reg(REG) ({ uint64_t reg_read;\
		       asm volatile ("MRS %x[reg_read]," #REG \
			     : [reg_read] "=r" (reg_read) : ); \
		       reg_read; })
#define write_sys_reg(REG, value) ({ uint64_t reg_write = ((uint64_t )(value));\
		       asm volatile ("dsb sy\n\t\
				     MSR " #REG ", %x[reg_write] \n\t\
				     isb" \
				     : \
				     : [reg_write] "r" (reg_write)); \
		       })

/**
 * struct arm_smccc_res - Result from SMC/HVC call
 * @a0-a3 result values from registers 0 to 3
 */
struct arm_smccc_res {
	unsigned long a0;
	unsigned long a1;
	unsigned long a2;
	unsigned long a3;
};

void arm_smccc_hvc(unsigned long a0, unsigned long a1, unsigned long a2,
		   unsigned long a3, unsigned long a4, unsigned long a5,
		   unsigned long a6, unsigned long a7,
		   struct arm_smccc_res *res);

#define PSCI_0_2_FN_PSCI_VERSION	0x84000000
#define PSCI_0_2_FN64_CPU_ON		0xC4000003

#define NULL ((void *)0)
enum {
	false = 0,
	true = 1
};

static inline int get_cpu_core_id(void)
{
	return read_reg(MPIDR_EL1) & 0xFF;
}

typedef void (*isr_func_t)(void);
typedef void (*syscall_func_t)(struct pt_regs *regs);

#define MAX_NUM_SYSCALLS 64

#define MAX_NUM_INTERRUPTS 64
enum {
	IRQ_TIMER = 30,
	IRQ_UART = 33,
	IRQ_RTC = 34,

	IRQ_SPURIOUS = 1023
};

enum {
	INT_TYPE_LEVEL = 0,
	INT_TYPE_EDGE
};

static inline void enable_irq(void)
{
	asm volatile (
		      "MSR DAIFClr, 0x2\n\t"
		      );
}

static inline void disable_irq(void)
{
	asm volatile (
		      "MSR DAIFSet, 0x2\n\t"
		      );
}

#define local_irq_save(flags) \
    do { \
        asm volatile ( \
            "MRS %x[reg_flags], DAIF\n\t" \
            :[reg_flags]"=r"(flags) \
        ); \
        disable_irq(); \
    } while (0);

#define local_irq_restore(flags) \
    do { \
        asm volatile ( \
            "MSR DAIF, %x[reg_flags]\n\t" \
            : \
            :[reg_flags]"r"(flags) \
        ); \
    } while (0);

static inline void invalidate_tlb(void)
{
	asm volatile (
		     "DSB ISHST\n\t"
		     "TLBI VMALLE1\n\t"
		     "DSB ISH\n\t"
		     "ISB\n\t"
		     );
}

static inline void invalidate_tlb_by_asid(int asid)
{
	asm volatile("dsb ishst; tlbi aside1, %0; dsb ish; isb" : : "r"((u64)asid << 48));
}

static inline void invalidate_tlb_by_va(int asid, void *virt_addr)
{
	asm volatile("dsb ishst; tlbi vae1, %0; dsb ish; isb" : : "r"(((u64)asid << 48) | (((u64)virt_addr) >> 12)));
}

static inline void write_ttbr0_el1(u64 pg_dir_phy_addr, int asid)
{
	write_sys_reg(TTBR0_EL1, ((u64)asid << 48) | pg_dir_phy_addr);
}

static inline int user_mode(const struct pt_regs *regs)
{
	return (((regs)->pstate & PSR_MODE_MASK) == PSR_MODE_EL0t);
}

#endif
