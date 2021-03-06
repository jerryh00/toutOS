#ifndef _STDLIB_H
#define _STDLIB_H

void init_malloc_free(void);
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);

#endif
