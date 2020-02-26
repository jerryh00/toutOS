#include <softirq.h>
#include <misc.h>
#include <percpu.h>
#include <printk.h>

struct softirq_action {
	void (*action)(void);
};

static struct softirq_action softirq_vec[MAX_NUM_SOFTIRQ];

DEFINE_PER_CPU(int, nested_softirq_count);
DEFINE_PER_CPU(int, __softirq_pending);

void open_softirq(int nr, void (*action)(void))
{
	if (!(nr >= 0 && nr < MAX_NUM_SOFTIRQ)) {
		printk("%s: nr is invalid, nr=%d\n", __FUNCTION__, nr);
		return;
	}
	if (action == NULL) {
		printk("%s: action is null\n", __FUNCTION__);
		return;
	}

	softirq_vec[nr].action = action;
}

void raise_softirq_irqoff(unsigned int nr)
{
	if (!(nr >= 0 && nr < MAX_NUM_SOFTIRQ)) {
		printk("%s: nr is invalid, nr=%d\n", __FUNCTION__, nr);
		return;
	}

	per_cpu(__softirq_pending, get_cpu_core_id()) |= (1 << nr);
}

void raise_softirq(unsigned int nr)
{
	unsigned long flags;

	if (!(nr >= 0 && nr < MAX_NUM_SOFTIRQ)) {
		printk("%s: nr is invalid, nr=%d\n", __FUNCTION__, nr);
		return;
	}

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}

static int is_softirq_pending(int softirq_pending, unsigned int nr)
{
	return ((softirq_pending & (1 << nr)) != 0);
}

void do_softirq(void)
{
	int i;
	unsigned long flags;
	int softirq_pending;

	local_irq_save(flags);
	if (per_cpu(nested_softirq_count, get_cpu_core_id()) != 0) {
		local_irq_restore(flags);
		return;
	}
	per_cpu(nested_softirq_count, get_cpu_core_id())++;
	softirq_pending = per_cpu(__softirq_pending, get_cpu_core_id());
	per_cpu(__softirq_pending, get_cpu_core_id()) = 0;
	local_irq_restore(flags);

	for (i = 0; i < MAX_NUM_SOFTIRQ; i++) {
		if (is_softirq_pending(softirq_pending, i)) {
			softirq_vec[i].action();
		}
	}

	local_irq_save(flags);
	per_cpu(nested_softirq_count, get_cpu_core_id())--;
	local_irq_restore(flags);
}
