#include <timer.h>
#include <string.h>
#include <misc.h>
#include <hw_timer.h>
#include <spinlock.h>
#include <printk.h>

static struct spinlock timerlist_lock;
static struct timer *head;

void init_timer_module(void)
{
	spin_lock_init(&timerlist_lock);
}

void init_timer(struct timer *p)
{
	if (p == NULL) {
		printk("%s: timer is null\n", __FUNCTION__);
		return;
	}

	memset(p, 0, (sizeof (*p)));
}

static void _add_timer(struct timer *p)
{
	p->next = head;
	head = p;
}

void add_timer(struct timer *p)
{
	unsigned long flags;

	if (p == NULL) {
		printk("%s: timer is null\n", __FUNCTION__);
		return;
	}

	flags = spin_lock_irqsave(&timerlist_lock);
	_add_timer(p);
	spin_unlock_irqrestore(&timerlist_lock, flags);
}

void _del_timer(struct timer *p)
{
	struct timer *cur;
	struct timer *prev;

	for (prev = NULL, cur = head; cur != NULL; prev = cur, cur = cur->next) {
		if (cur == p) {
			if (prev != NULL) {
				prev->next = cur->next;
			}
			if (head == cur) {
				head = cur->next;
			}
			cur->next = NULL;
			break;
		}
	}
}

void del_timer(struct timer *p)
{
	unsigned long flags;

	if (p == NULL) {
		printk("%s: timer is null\n", __FUNCTION__);
		return;
	}

	flags = spin_lock_irqsave(&timerlist_lock);
	_del_timer(p);
	spin_unlock_irqrestore(&timerlist_lock, flags);
}

static void _mod_timer(struct timer *p, unsigned long expires)
{
	_del_timer(p);
	p->expires = expires;
	_add_timer(p);
}

void mod_timer(struct timer *p, unsigned long expires)
{
	unsigned long flags;

	if (p == NULL) {
		printk("%s: timer is null\n", __FUNCTION__);
		return;
	}

	flags = spin_lock_irqsave(&timerlist_lock);
	_mod_timer(p, expires);
	spin_unlock_irqrestore(&timerlist_lock, flags);
}

void run_timer_softirq(void)
{
	unsigned long tick;
	struct timer *cur;
	unsigned long flags;

#ifdef DEBUG_TIMER_SOFTIRQ
	printk("%s\n", __FUNCTION__);
#endif
	tick = get_tick();

	flags = spin_lock_irqsave(&timerlist_lock);
	cur = head;
	while (cur != NULL) {
		if (cur->expires == tick) {
			_del_timer(cur);
			spin_unlock_irqrestore(&timerlist_lock, flags);
			cur->function(cur->data);
			flags = spin_lock_irqsave(&timerlist_lock);
			cur = head;
		} else {
			cur = cur->next;
		}
	}
	spin_unlock_irqrestore(&timerlist_lock, flags);
}
