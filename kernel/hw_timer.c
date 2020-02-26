#include <hw_timer.h>
#include <arch.h>
#include <printk.h>
#include <misc.h>
#include <softirq.h>
#include <timer.h>

static uint64_t tick;

void config_hw_timer(void)
{
	uint32_t reg;

	reg = read_reg(CNTFRQ_EL0);
	printk("CNTFRQ_EL0=%d\n", reg);

	reg = read_reg(CNTP_CVAL_EL0);
	printk("CNTP_CVAL_EL0=%d\n", reg);

	reg = read_reg(CNTPCT_EL0);
	printk("CNTPCT_EL0=%d\n", reg);

	reg = read_reg(CNTP_CTL_EL0);
	printk("CNTP_CTL_EL0=%d\n", reg);

	write_sys_reg(CNTP_TVAL_EL0, TICK_TIMER_COUNT);
	write_sys_reg(CNTP_CTL_EL0, 1);

	open_softirq(SOFTIRQ_TIMER, run_timer_softirq);
}

void handle_timer_irq(void)
{
	write_sys_reg(CNTP_TVAL_EL0, TICK_TIMER_COUNT);
#ifdef DEBUG_TIMER_IRQ
	printk("core %d: Got timer interrupt.\n", get_cpu_core_id());
	dump_stack();
#endif
	if (get_cpu_core_id() == 0) {
		tick++;
	}

	raise_softirq(SOFTIRQ_TIMER);
}

uint64_t get_tick(void)
{
	return tick;
}
