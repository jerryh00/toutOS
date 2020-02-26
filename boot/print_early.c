#include <print_early.h>
#include <hardware.h>
#include <print.h>

static int print_char(char c)
{
	*(unsigned int *)UART_UARTDR = c;

	return 1;
}

int print_early(const char *fmt, ...)
{
	int num_printed = 0;
	va_list ap;
	int d;
	void *p;
	char c, *s;
	int width;
	char pad;

	if (fmt == NULL) {
		return 0;
	}

	PRINT

	return num_printed;
}
