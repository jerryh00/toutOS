#include "../include/asm.h"
#include "../include/hardware.h"

/* re. Linux */

#define S_FRAME_SIZE 288 /* sizeof(struct pt_regs)	// */
#define S_LR 240 /* offsetof(struct pt_regs, regs[30])	// */
#define S_SP 248 /* offsetof(struct pt_regs, sp)	// */
#define S_PC 256 /* offsetof(struct pt_regs, pc)	// */

/* offsetof(struct task_struct, thread.cpu_context)); */
#define THREAD_CPU_CONTEXT 16

.globl spin_lock, spin_unlock, call_thread_func, switch_to_user_mode, child_returns_from_fork

/*
 * Enable and disable interrupts.
 */
	.macro	disable_irq
	msr	daifset, #2
	.endm

	.macro	enable_irq
	msr	daifclr, #2
	.endm

	.macro	kernel_entry, el
	sub	sp, sp, #S_FRAME_SIZE

	stp	x0, x1, [sp, #16 * 0]
	stp	x2, x3, [sp, #16 * 1]
	stp	x4, x5, [sp, #16 * 2]
	stp	x6, x7, [sp, #16 * 3]
	stp	x8, x9, [sp, #16 * 4]
	stp	x10, x11, [sp, #16 * 5]
	stp	x12, x13, [sp, #16 * 6]
	stp	x14, x15, [sp, #16 * 7]
	stp	x16, x17, [sp, #16 * 8]
	stp	x18, x19, [sp, #16 * 9]
	stp	x20, x21, [sp, #16 * 10]
	stp	x22, x23, [sp, #16 * 11]
	stp	x24, x25, [sp, #16 * 12]
	stp	x26, x27, [sp, #16 * 13]
	stp	x28, x29, [sp, #16 * 14]

	.if \el == 0
	mrs     x21, sp_el0
	.else
	add	x21, sp, #S_FRAME_SIZE
	.endif

	mrs	x22, elr_el1
	mrs	x23, spsr_el1
	stp	lr, x21, [sp, #S_LR]
	stp	x22, x23, [sp, #S_PC]

	/*
	 * Registers that may be useful after this macro is invoked:
	 *
	 * x21 - aborted SP
	 * x22 - aborted PC
	 * x23 - aborted PSTATE
	*/
	.endm

	.macro	kernel_exit, el
	ldp	x21, x22, [sp, #S_PC]		// load ELR, SPSR

	.if \el == 0
        ldr     x23, [sp, #S_SP]                // load return stack pointer
        msr     sp_el0, x23
	.endif

	msr	elr_el1, x21			// set up the return data
	msr	spsr_el1, x22

	ldp	x0, x1, [sp, #16 * 0]
	ldp	x2, x3, [sp, #16 * 1]
	ldp	x4, x5, [sp, #16 * 2]
	ldp	x6, x7, [sp, #16 * 3]
	ldp	x8, x9, [sp, #16 * 4]
	ldp	x10, x11, [sp, #16 * 5]
	ldp	x12, x13, [sp, #16 * 6]
	ldp	x14, x15, [sp, #16 * 7]
	ldp	x16, x17, [sp, #16 * 8]
	ldp	x18, x19, [sp, #16 * 9]
	ldp	x20, x21, [sp, #16 * 10]
	ldp	x22, x23, [sp, #16 * 11]
	ldp	x24, x25, [sp, #16 * 12]
	ldp	x26, x27, [sp, #16 * 13]
	ldp	x28, x29, [sp, #16 * 14]
	ldr	lr, [sp, #S_LR]
	add	sp, sp, #S_FRAME_SIZE		// restore sp
	eret					// return to kernel
	.endm

/*
 * Vector entry
 */
	.macro ventry  label
	.align  7
	b       \label
	.endm

/*
 * Exception vectors.
 */
	.align 11
ENTRY(vectors)
	ventry handle_unknown_interrupt
	ventry handle_unknown_interrupt
	ventry handle_unknown_interrupt
	ventry handle_unknown_interrupt

	ventry el1_sync
	ventry el1_irq
	ventry handle_unknown_interrupt
	ventry handle_unknown_interrupt

	ventry el0_sync
	ventry el0_irq
	ventry handle_unknown_interrupt
	ventry handle_unknown_interrupt

	ventry handle_unknown_interrupt
	ventry handle_unknown_interrupt
	ventry handle_unknown_interrupt
	ventry handle_unknown_interrupt
END(vectors)

	.align 6
handle_unknown_interrupt:
	# save registers
	b do_unknown_interrupt

	.align 6
el1_sync:
	kernel_entry 1
	enable_irq
	mrs x1, esr_el1
	mrs x0, far_el1
	mov x2, sp
	bl do_el1_sync
	disable_irq
	kernel_exit 1

	.align 6
el1_irq:
	kernel_entry 1
	mov x0, sp
	bl irq_handler
	kernel_exit 1

	.align 6
el0_sync:
	kernel_entry 0
	enable_irq
	mrs x1, esr_el1
	mrs x0, far_el1
	mov x2, sp
	bl do_el0_sync
	disable_irq
	kernel_exit 0

	.align 6
el0_irq:
	kernel_entry 0
	mov x0, sp
	bl irq_handler
	kernel_exit 0

#ifndef DEBUG_SPINLOCK_NULLIFY
#ifndef SPIN_LOCK_IN_C
spin_lock:
	MRS X7, DAIF
	MSR DAIFSet, 0x2

	LDXR X1, [X0]
	CBZ X1, _try_lock

	MSR DAIF, X7
	B spin_lock
_try_lock:
	MOV X1, 1
	STXR W2, X1, [X0]
	MSR DAIF, X7
	CBNZ X2, spin_lock
	ret

spin_unlock:
	MRS X7, DAIF
	MSR DAIFSet, 0x2
	LDXR X1, [X0]
	CBZ X1, _un_locked

	MOV X1, 0
	STXR W2, X1, [X0]
_un_locked:
	MSR DAIF, X7
	ret
#endif
#endif

/* arch/arm64/kernel/entry.S */
/*
 * Register switch for AArch64. The callee-saved registers need to be saved
 * and restored. On entry:
 *   x0 = previous task_struct (must be preserved across the switch)
 *   x1 = next task_struct
 * Previous and next are guaranteed not to be the same.
 *
 */
ENTRY(cpu_switch_to)
	mov	x10, #THREAD_CPU_CONTEXT
	add	x8, x0, x10
	mov	x9, sp
	stp	x19, x20, [x8], #16		// store callee-saved registers
	stp	x21, x22, [x8], #16
	stp	x23, x24, [x8], #16
	stp	x25, x26, [x8], #16
	stp	x27, x28, [x8], #16
	stp	x29, x9, [x8], #16
	str	lr, [x8]
	add	x8, x1, x10
	ldp	x19, x20, [x8], #16		// restore callee-saved registers
	ldp	x21, x22, [x8], #16
	ldp	x23, x24, [x8], #16
	ldp	x25, x26, [x8], #16
	ldp	x27, x28, [x8], #16
	ldp	x29, x9, [x8], #16
	ldr	lr, [x8]
	mov	sp, x9
	ret
ENDPROC(cpu_switch_to)

call_thread_func:
	ldp x0, x1, [sp], #16
	blr x1
	bl do_exit
	b cpu_idle

switch_to_user_mode:
	MRS X2, SPSR_EL1
	AND X2, X2, #~0xF
	MSR SPSR_EL1, X2
	MSR ELR_EL1, LR
	CBZ X0, set_user_stack
	MSR ELR_EL1, X0
set_user_stack:
	CBZ X1, set_user_stack_end
	MSR SP_EL0, X1
set_user_stack_end:
	ERET

child_returns_from_fork:
	MSR DAIFSet, 0x2
	kernel_exit 0
