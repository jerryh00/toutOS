#include <mutex.h>
#include <sched.h>
#include <printk.h>

void init_mutex(mutex_t *p)
{
	if (p == NULL) {
		printk("%s: lock is null\n", __FUNCTION__);
		return;
	}

	spin_lock_init(&p->lock);
	p->state = MUTEX_UNLOCKED;
	p->waiting = 0;
}

void mutex_lock(mutex_t *p)
{
	if (p == NULL) {
		printk("%s: lock is null\n", __FUNCTION__);
		return;
	}

	while (true) {
		spin_lock(&p->lock);

		if (p->state == MUTEX_UNLOCKED) {
			p->state = MUTEX_LOCKED;
			spin_unlock(&p->lock);
			break;
		} else {
			struct task_struct *current = get_current_proc();

			p->waiting |= (1ULL << (unsigned int)current->pid);
			set_task_state(current, SLEEPING);
			spin_unlock(&p->lock);
			schedule();
		}
	}
}

static int get_next_waiting(mutex_t *m)
{
	int i;
	static int max_pid = ((sizeof (m->waiting)) * 8);

	for (i = 0; i < max_pid; i++) {
		if ((m->waiting & (1ULL << (unsigned int)i)) != 0) {
			return i;
		}
	}

	return -1;
}

void mutex_unlock(mutex_t *p)
{
	int pid;

	if (p == NULL) {
		printk("%s: lock is null\n", __FUNCTION__);
		return;
	}

	spin_lock(&p->lock);
	p->state = MUTEX_UNLOCKED;
	pid = get_next_waiting(p);
	if (pid > 0) {
		p->waiting &= ~(1ULL << (unsigned int)pid);
		set_task_state(pid_to_task(pid), RUNNING);
	}
	spin_unlock(&p->lock);
}
