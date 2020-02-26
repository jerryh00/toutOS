#ifndef __WAIT_H
#define __WAIT_H

#include <list.h>
#include <sched.h>
#include <spinlock.h>

struct wait_queue_entry {
	struct task_struct *task;
	struct list_head entry;
};

struct wait_queue_head {
	struct spinlock	lock;
	struct list_head head;
};

static inline void init_waitqueue_head(struct wait_queue_head *wq_head)
{
	spin_lock_init(&wq_head->lock);
	INIT_LIST_HEAD(&wq_head->head);
}

static inline void init_waitqueue_entry(struct wait_queue_entry *wq_entry, struct task_struct *p)
{
	wq_entry->task = p;
}

static inline void add_wait_queue(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry)
{
	unsigned long flags;

	flags = spin_lock_irqsave(&wq_head->lock);
	list_add(&wq_entry->entry, &wq_head->head);
	spin_unlock_irqrestore(&wq_head->lock, flags);
}

static inline void remove_wait_queue(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry)
{
	unsigned long flags;

	flags = spin_lock_irqsave(&wq_head->lock);
	list_del(&wq_entry->entry);
	spin_unlock_irqrestore(&wq_head->lock, flags);
}

void wake_up(struct wait_queue_head *wq_head);

#endif
