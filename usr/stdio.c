#include <stdio.h>
#include <hardware.h>
#include <print.h>
#include <mmu.h>

static int print_char(char c)
{
	*(unsigned int *)__uva(UART_UARTDR) = c;

	return 1;
}

int printf(const char *fmt, ...)
{
	int num_printed = 0;
	va_list ap;
	int d;
	void *p;
	char c, *s;
	int width;
	char pad;

	PRINT

	return num_printed;
}
