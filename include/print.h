#ifndef __PRINT_H__
#define __PRINT_H__

#include <stddef.h>

#define NUM_HEX_FOR_PTR ((sizeof (void *))*2)

static int print_char(char c);

static inline int print_string(const char *p)
{
	int i = 0;

	if (p == NULL) {
		return 0;
	}

	while (*(p + i) != '\0') {
		print_char(*(p + i));
		i++;
	}

	return i;
}

static inline int print_uint64_t(uint64_t d, int base, int width, char pad)
{
	static char digits[] = "0123456789ABCDEF";

	uint64_t quotient, rem;
	int num_printed = 0;
	char buf[64];
	int num_digit = 0;
	int i;
	int j;
	int num_pad;

	do {
		quotient = d / base;
		rem = d % base;
		buf[num_digit++] = digits[rem];
		d = quotient;
	} while (d != 0);

	num_pad = width - num_digit;
	for (j = 0; j < num_pad; j++) {
		print_char(pad);
		num_printed += 1;
	}

	for (i = num_digit - 1; i >= 0; i--) {
		print_char(buf[i]);
		num_printed += 1;
	}

	return num_printed;
}

static inline int print_ptr(const void *p)
{
	int num_printed = 0;
	int base = 16;
	int width = NUM_HEX_FOR_PTR;
	char pad = '0';

#ifdef ARM64
	/* same as fprintf %p, print as unsigned. */
	num_printed += print_uint64_t((uintptr_t)p, base, width, pad);
#endif

	return num_printed;
}

static inline int print_int32_t(int32_t d)
{
	int num_printed = 0;
	int base = 10;
	int width = -1;
	char pad = -1;

	if (d < 0) {
		num_printed += print_char('-');
		d = -d;
	}

	num_printed += print_uint64_t(d, base, width, pad);

	return num_printed;
}

static inline int print_hex(int64_t d)
{
	int num_printed = 0;
	int base = 16;
	int width = -1;
	char pad = -1;

	if (d < 0) {
		num_printed += print_char('-');
		d = -d;
	}

	num_printed += print_uint64_t(d, base, width, pad);

	return num_printed;
}

static inline int print_unsigned_int(unsigned int d, int base, int width, char pad)
{
	static char digits[] = "0123456789ABCDEF";

	int quotient, rem;
	int num_printed = 0;
	char buf[32];
	int num_digit = 0;
	int i;
	int j;
	int num_zero;

	assert(base <= 16);

	do {
		quotient = d / base;
		rem = d % base;
		buf[num_digit++] = digits[rem];
		d = quotient;
	} while (d != 0);

	num_zero = width - num_digit;
	for (j = 0; j < num_zero; j++) {
		print_char(pad);
		num_printed += 1;
	}

	for (i = num_digit - 1; i >= 0; i--) {
		print_char(buf[i]);
		num_printed += 1;
	}

	return num_printed;
}

static inline int print_int(int d, int base, int width, char pad)
{
	int num_printed = 0;

	assert(base <= 16);

	if (d < 0) {
		num_printed += print_char('-');
		d = -d;
	}

	num_printed += print_unsigned_int(d, base, width, pad);

	return num_printed;
}

#define PRINT \
	va_start0(ap, fmt); \
	for (;*fmt != '\0'; fmt++) { \
		if (*fmt != '%') { \
			print_char(*fmt); \
			num_printed += 1; \
			continue; \
		} \
		fmt += 1; \
 \
		if (*fmt == '\0') { \
			break; \
		} \
 \
		pad = ' '; \
		if (*fmt == '0') { \
			pad = '0'; \
			fmt += 1; \
			if (*fmt == '\0') { \
				break; \
			} \
		} \
 \
		width = -1; \
		if (*fmt >= '1' && *fmt <= '9') { \
			width = (*fmt) - '0'; \
			fmt += 1; \
			if (*fmt == '\0') { \
				break; \
			} \
		} \
 \
		switch (*fmt) { \
		case 's': /* string */ \
			s = va_arg(ap, char *); \
			num_printed += print_string(s); \
			break; \
		case 'd': /* signed decimal */ \
			d = va_arg(ap, int); \
			num_printed += print_int(d, 10, width, pad); \
			break; \
		case 'u': /* unsigned decimal */ \
			d = va_arg(ap, int); \
			num_printed += print_unsigned_int(d, 10, width, pad); \
			break; \
		case 'x': /* hex int */ \
			d = va_arg(ap, int); \
			/* same as fprintf %x, print as unsigned. */ \
			num_printed += print_unsigned_int(d, 16, width, pad); \
			break; \
		case 'o': /* octal int */ \
			d = va_arg(ap, int); \
			/* same as fprintf %o, print as unsigned. */ \
			num_printed += print_unsigned_int(d, 8, width, pad); \
			break; \
		case 'p': /* pointer */ \
			p = va_arg(ap, void *); \
			num_printed += print_ptr(p); \
			break; \
		case 'c': /* char */ \
			c = va_arg(ap, char); \
			num_printed += print_char(c); \
			break; \
		default: \
			assert(0); \
		} \
	} \
	va_end(ap);

#endif
