#include <spinlock.h>
#include <string.h>
#include <printk.h>
#include <arch.h>
#include <printk.h>
#include <stddef.h>

void spin_lock_init(struct spinlock *lock)
{
	if (lock == NULL) {
		printk("%s: lock is null\n", __FUNCTION__);
		return;
	}

	memset(lock, 0, sizeof (*lock));
}

#ifdef DEBUG_SPINLOCK_NULLIFY
void spin_lock(struct spinlock *lock)
{
}
void spin_unlock(struct spinlock *lock)
{
}
#elif defined SPIN_LOCK_IN_C
static u64 ldxr_func(void *addr)
{
	u64 ret;

	asm volatile (
		      "LDXR x1, [%x[input_addr]]\n\t"
		      "str x1, %x[output_ret]"
		      : [output_ret] "=m" (ret)
		      : [input_addr] "r" (addr)
		     );
	return ret;
}

static u64 stxr_func(u64 value, void *addr)
{
	u64 ret;

	asm volatile (
		      "STXR w2, %x[input_value], [%x[input_addr]]\n\t"
		      "str x2, %x[output_ret]\n\t"
		      "DMB ISH\n\t"
		      : [output_ret] "=m" (ret)
		      :  [input_value] "r" (value), [input_addr] "r" (addr)
		     );

	return ret;
}

void spin_lock(struct spinlock *lock)
{
	u64 state;
	unsigned long flags;

	if (lock == NULL) {
		printk("%s: lock is null\n", __FUNCTION__);
		return;
	}

	while (true) {
		local_irq_save(flags);
		state = ldxr_func(lock);
		if (state == 0) {
			int ret;

			state = 1;
			ret = stxr_func(state, lock);
			if (ret == 0) {
				local_irq_restore(flags);
				return;
			}
		}
		local_irq_restore(flags);
	}
}

void spin_unlock(struct spinlock *lock)
{
	u64 state;
	unsigned long flags;

	if (lock == NULL) {
		printk("%s: lock is null\n", __FUNCTION__);
		return;
	}

	local_irq_save(flags);
	state = ldxr_func(lock);
	if (state == 1) {
		state = 0;
		(void)stxr_func(state, lock);
	}
	local_irq_restore(flags);
}
#endif

void print_spin_lock(struct spinlock *lock)
{
	if (lock == NULL) {
		printk("%s: lock is null\n", __FUNCTION__);
		return;
	}

	printk("spinlock@%p\n", lock);
	printk("%p, %p\n", lock->data[0], lock->data[1]);
}

unsigned long spin_lock_irqsave(struct spinlock *lock)
{
	unsigned long flags;

	assert(lock != NULL);

	local_irq_save(flags);
	spin_lock(lock);

	return flags;
}

void spin_unlock_irqrestore(struct spinlock *lock, unsigned long flags)
{
	if (lock == NULL) {
		printk("%s: lock is null\n", __FUNCTION__);
		return;
	}

	spin_unlock(lock);
	local_irq_restore(flags);
}
