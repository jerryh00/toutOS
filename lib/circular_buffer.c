#include <circular_buffer.h>
#include <misc.h>
#include <printk.h>

void init_circular_buffer(struct circular_buffer *p, const void *buffer,
			  size_t length)
{
	if (p == NULL || buffer == NULL) {
		return;
	}

	p->buffer = (char *)buffer;
	p->length = length;
	p->rp = (char *)buffer;
	p->wp = (char *)buffer;
}

int write_circular_buffer(struct circular_buffer *p, const void *data,
			  size_t length)
{
	char *begin = p->buffer;
	char *end = p->buffer + p->length;
	char *rp = p->rp;
	char *wp = p->wp;
	const char *cur_data;

	if (p == NULL || data == NULL) {
		return -1;
	}

	if (wp >= rp) {
		if (p->length - 1 - (wp - rp) < length) {
			return -1;
		}
	} else {
		if (rp -1 - wp < length) {
			return -1;
		}
	}
	cur_data = (const char *)data;
	while (length--) {
		*wp++ = *cur_data++;
		if (wp == end) {
			wp = begin;
		}
	}
	/* rp/wp pointer update should happen only after memory copy */
	p->wp = wp;

	return 0;
}

int read_circular_buffer(struct circular_buffer *p, void *data, size_t length)
{
	char *begin = p->buffer;
	char *end = p->buffer + p->length;
	char *rp = p->rp;
	char *wp = p->wp;
	char *cur_data;
	size_t store_len;
	size_t to_read_len;

	if (p == NULL || data == NULL) {
		return -1;
	}

	if (wp >= rp) {
		store_len = wp - rp;
	} else {
		store_len = p->length - (rp - wp);
	}
	to_read_len = min(length, store_len);
	cur_data = (char *)data;
	for(int i = 0; i < to_read_len; i++) {
		*cur_data++ = *rp++;
		if (rp == end) {
			rp = begin;
		}
	}
	/* rp/wp pointer update should happen only after memory copy */
	p->rp = rp;

	return to_read_len;
}

void init_concurrent_cbuf(struct concurrent_cbuf *p, const void *buffer,
			  size_t length)
{
	if (p == NULL) {
		printk("%s: concurrent_cbuf is null\n", __FUNCTION__);
		return;
	}
	if (buffer == NULL) {
		printk("%s: buffer is null\n", __FUNCTION__);
		return;
	}

	spin_lock_init(&p->cbuf_rlock);
	spin_lock_init(&p->cbuf_wlock);
	init_circular_buffer(&p->cbuf, buffer, length);
	p->write_overflow = false;
}

int read_concurrent_cbuf(struct concurrent_cbuf *cbuf, char *p, size_t length)
{
	int ret;
	unsigned long flags;

	if (cbuf == NULL) {
		printk("%s: concurrent_cbuf is null\n", __FUNCTION__);
		return -1;
	}
	if (p == NULL) {
		printk("%s: buffer is null\n", __FUNCTION__);
		return -1;
	}
	flags = spin_lock_irqsave(&cbuf->cbuf_rlock);
	ret = read_circular_buffer(&cbuf->cbuf, p, length);
	spin_unlock_irqrestore(&cbuf->cbuf_rlock, flags);

	return ret;
}

int write_concurrent_cbuf(struct concurrent_cbuf *cbuf, const char *p,
			  size_t length)
{
	int ret;
	unsigned long flags;

	if (cbuf == NULL) {
		printk("%s: concurrent_cbuf is null\n", __FUNCTION__);
		return -1;
	}
	if (p == NULL) {
		printk("%s: buffer is null\n", __FUNCTION__);
		return -1;
	}

	flags = spin_lock_irqsave(&cbuf->cbuf_wlock);
	ret = write_circular_buffer(&cbuf->cbuf, p, length);
	if (ret < 0) {
		cbuf->write_overflow = true;
	}
	spin_unlock_irqrestore(&cbuf->cbuf_wlock, flags);

	return ret;
}
