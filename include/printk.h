#ifndef __PRINTK_H__
#define __PRINTK_H__

void init_printk(void);

int printk(const char *fmt, ...);

void test_printk(void);

#endif
