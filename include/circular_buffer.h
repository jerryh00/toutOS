#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spinlock.h>

struct circular_buffer {
	char *buffer;
	size_t length;
	char *rp;
	char *wp;
};

void init_circular_buffer(struct circular_buffer *p, const void *buffer,
			  size_t length);
int write_circular_buffer(struct circular_buffer *p, const void *data,
			  size_t length);
int read_circular_buffer(struct circular_buffer *p, void *data, size_t length);

struct concurrent_cbuf {
	struct circular_buffer cbuf;
	struct spinlock cbuf_rlock;
	struct spinlock cbuf_wlock;
	int write_overflow;
};

void init_concurrent_cbuf(struct concurrent_cbuf *p, const void *buffer,
			  size_t length);
int read_concurrent_cbuf(struct concurrent_cbuf *cbuf, char *p, size_t length);
int write_concurrent_cbuf(struct concurrent_cbuf *cbuf, const char *p,
			  size_t length);

#ifdef __cplusplus
}
#endif

#endif
