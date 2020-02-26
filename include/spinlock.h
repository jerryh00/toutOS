#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

struct spinlock {
	/* CTR_EL0.ERG = 4 for QEMU_VIRT */
	u64 data[2] __attribute__ ((aligned (16)));
};

void spin_lock_init(struct spinlock *lock);
void spin_lock(struct spinlock *lock);
void spin_unlock(struct spinlock *lock);

void print_spin_lock(struct spinlock *lock);

unsigned long spin_lock_irqsave(struct spinlock *lock);
void spin_unlock_irqrestore(struct spinlock *lock, unsigned long flags);

#endif
