#ifndef _SOFTIRQ_H
#define _SOFTIRQ_H

enum {
	SOFTIRQ_TIMER = 0,
	MAX_NUM_SOFTIRQ
};

extern void open_softirq(int nr, void (*action)(void));
extern void raise_softirq_irqoff(unsigned int nr);
extern void raise_softirq(unsigned int nr);

extern void do_softirq(void);

#endif
