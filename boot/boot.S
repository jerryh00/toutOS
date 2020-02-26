#include "../include/hardware.h"
#include "../include/asm.h"

.text

.globl	_start
_start:
	/* Check CPU ID */
	mrs x0, mpidr_el1
	tst x0, #0xFF
	b.ne    other_cpu

boot_cpu:
	bl el2_setup

	/* Configure stack */
	ldr x0, =swapper_stacks
	add x0, x0, #PAGE_SIZE
	mov sp, x0

#ifdef TEST_PHY_MEM
	bl test_phy_mem
#endif
#ifdef QEMU_VIRT
	/* Init UART */
        ldr     x4, =UART_BASE
        mov     w5, #0x0D	/* 24MHZ/(16*115200)=13.020833333333334 */
        str     w5, [x4, #0x24]
        mov     w5, #0x02    /* 0.020833333333334*64+0.5=1.8333333333333712 */
        str     w5, [x4, #0x28]
        mov     w5, #0x0301
        str     w5, [x4, #0x30]
#endif

	bl clear_bss
	bl add_map

	bl enable_paging

	/* Configure stack to linear address */
	ldr x0, =main
	and x0, x0, #~(0xFFFFFFFF)
	add sp, sp, x0

	ldr x0, =vectors
	msr vbar_el1, x0

	b main

other_cpu:
	bl el2_setup

	mrs x1, mpidr_el1
	and x1, x1, #0xFF

	/* Configure stack */
	ldr x0, =swapper_stacks
	add x0, x0, #PAGE_SIZE
loop_set_stack:
	add x0, x0, #PAGE_SIZE
	sub x1, x1, #1
	cbnz x1, loop_set_stack

	mov sp, x0

	bl enable_paging
	/* configure stack to linear address */
	ldr x0, =main
	and x0, x0, #~(0xFFFFFFFF)
	add sp, sp, x0

	ldr x0, =vectors
	msr vbar_el1, x0

	bl main_secondary_cpu

# re. Linux kernel head.S
ENTRY(el2_setup)
	mrs  x0, CurrentEL
	cmp x0, #0x08
	b.eq 1f
	ret

1:
	# Register Width control for lower exception levels: 1: EL1 is AArch64.
	mov x0, #(1 << 31)
	msr hcr_el2, x0

	/* sctlr_el1 */
	# write 1s to RES1 bits, leave other bits as zero.
	# Because RES1 means SBO, and SBO means
	# software should write the field as all 1s.
	mov x0, #0x0800
	movk x0,#0x30d0, lsl #16

	msr  sctlr_el1, x0

	/* Coprocessor traps. */
	# disable Float, trace traps.
	mov x0, #0x33ff
	msr  cptr_el2, x0

	# Controls access to T32EE and coprocessor registers at lower exception levels in AArch32
	msr  hstr_el2, xzr

	/* spsr */
	mov x0, #0x3c5
	msr spsr_el2, x0

	msr elr_el2, lr

	eret
ENDPROC(el2_setup)

.macro SMCCC instr
.cfi_startproc
	\instr	#0
	ldr	x4, [sp]
	stp	x0, x1, [x4, #0]
	stp	x2, x3, [x4, #16]
	ret
.cfi_endproc
.endm

/*
 * void arm_smccc_hvc(unsigned long a0, unsigned long a1, unsigned long a2,
 *		  unsigned long a3, unsigned long a4, unsigned long a5,
 *		  unsigned long a6, unsigned long a7, struct arm_smccc_res *res)
 */
ENTRY(arm_smccc_hvc)
	SMCCC	hvc
ENDPROC(arm_smccc_hvc)

/* For assembly debugging. */
ENTRY(putc)
	mov x1, #UART_BASE
	str x0, [x1]
	ret
