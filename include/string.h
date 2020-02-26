#ifndef _STRING_H
#define _STRING_H

static inline void *memset(void *s, int c, size_t n)
{
	size_t i;
	char *p = (char *)s;

	for (i = 0; i < n; i++) {
		p[i] = c;
	}

	return s;
}

static inline void *memcpy(void *dest, const void *src, size_t n)
{
	char *d = (char *)dest;
	char *s = (char *)src;
	size_t i;

	for (i = 0; i < n; i++) {
		d[i] = s[i];
	}

	return dest;
}

static inline char *strchr(const char *s, int c)
{
	int i;

	if (s == NULL) {
		return NULL;
	}

	for (i = 0; s[i] != '\0'; i++) {
		if (s[i] == c) {
			return (char *)&s[i];
		}
	}

	return NULL;
}

static inline int strcmp(const char *s1, const char *s2)
{
	while (true) {
		if (*s1 == '\0' && *s2 == '\0') {
			return 0;
		} else if (*s1 == '\0' && *s2 != '\0') {
			return -1;
		} else if (*s1 != '\0' && *s2 == '\0') {
			return 1;
		} else {
			if (*s1 == *s2) {
				s1++;
				s2++;
			} else if (*s1 < *s2) {
				return -1;
			} else {
				return 1;
			}
		}
	}

	return 0;
}

static inline int strncmp(const char *s1, const char *s2, size_t n)
{
	while (true) {
		if (n == 0) {
			return 0;
		}
		n--;
		if (*s1 == '\0' && *s2 == '\0') {
			return 0;
		} else if (*s1 == '\0' && *s2 != '\0') {
			return -1;
		} else if (*s1 != '\0' && *s2 == '\0') {
			return 1;
		} else {
			if (*s1 == *s2) {
				s1++;
				s2++;
			} else if (*s1 < *s2) {
				return -1;
			} else {
				return 1;
			}
		}
	}
}

static inline size_t strlen(const char *s)
{
	int i;

	for (i = 0; s[i] != '\0'; i++) {
	}

	return i;
}

static inline char *strcpy(char *dest, const char *src)
{
	size_t i;

	for (i = 0; src[i] != '\0'; i++) {
		dest[i] = src[i];
	}
	dest[i] = '\0';

	return dest;
}

/* From man strcpy. */
static inline char* strncpy(char *dest, const char *src, size_t n)
{
	size_t i;

	for (i = 0 ; i < n && src[i] != '\0'; i++)
		dest[i] = src[i];
	for ( ; i < n ; i++)
		dest[i] = '\0';

	return dest;
}

#endif
