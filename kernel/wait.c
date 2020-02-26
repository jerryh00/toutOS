#include <wait.h>

void wake_up(struct wait_queue_head *wq_head)
{
	unsigned long flags;
	struct wait_queue_entry *wq_entry;

	flags = spin_lock_irqsave(&wq_head->lock);

	list_for_each_entry(wq_entry, &wq_head->head, entry) {
		set_task_state(wq_entry->task, RUNNING);
	}

	spin_unlock_irqrestore(&wq_head->lock, flags);
}
