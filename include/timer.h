#ifndef _TIMER_H
#define _TIMER_H

struct timer {
	struct timer *next;
	unsigned long expires;
	void (*function)(unsigned long);
	unsigned long data;
};

extern void init_timer(struct timer *p);
extern void add_timer(struct timer *p);
extern void del_timer(struct timer *p);
extern void mod_timer(struct timer *p, unsigned long expires);

extern void run_timer_softirq(void);

void init_timer_module(void);

#endif
