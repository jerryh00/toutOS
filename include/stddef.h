#ifndef _STDDEF_H
#define _STDDEF_H

#include <hardware.h>

#define NULL ((void *)0)

#define assert(x) do { if (!(x)) { while (1) { asm volatile ("wfi"); } } } while (0)

typedef char *va_list;

/* re. include/acpi/platform/acenv.h, modified */
#define aligned_to_64_bit(X) (((sizeof (X)) + 7) & (~7))
#define aligned_to_arch aligned_to_64_bit

#define va_start0(ap, A) ((ap) = *(char **)((char *) &(A) - 24) - 56)
#define va_arg(ap, T) (*(T *)((ap += aligned_to_arch(sizeof (T))) - \
           aligned_to_arch(sizeof (T))))
#define va_end(ap) (void) 0

#define ALIGNED_TO_4BYTES(val) (((unsigned long)val + 3) / 4 * 4)
#define ALIGNED_TO_8BYTES(val) (((unsigned long)val + 7) / 8 * 8)

#define ENOSYS 38

static inline int get_order(size_t size)
{
	int i;

	for (i = 0; i < 64; i++) {
		if (((1UL << i) * PAGE_SIZE) >= size) {
			break;
		}
	}

	return i;
}

#endif
