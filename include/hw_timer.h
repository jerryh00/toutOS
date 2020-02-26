#ifndef __HW_TIMER_H__
#define __HW_TIMER_H__

#define TICK 10	/* ms */
#define HZ (1000 / TICK)
#define CNTFRQ_EL0_VALUE 62500000
#define TICK_TIMER_COUNT (CNTFRQ_EL0_VALUE / (1000/TICK))

void config_hw_timer(void);
void handle_timer_irq(void);
uint64_t get_tick(void);

#endif
