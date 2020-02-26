#ifndef _MM_H
#define _MM_H

struct mem_pool {
	void *start;
	void *end;
	void *(*sbrk_func)(intptr_t increment);
	size_t total_length;
	void *lock;
	void (*lock_init_func)(void *lock);
	void (*lock_func)(void *lock);
	void (*unlock_func)(void *lock);
};

#endif
