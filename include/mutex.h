#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <spinlock.h>

enum mutex_state {MUTEX_UNLOCKED=0, MUTEX_LOCKED};

typedef struct mutex {
	struct spinlock lock;
	int state;
	/* Use a bitmap for now, it's simpler. */
	unsigned int waiting; /* Bitmap for waiting processes. */
} mutex_t;

void init_mutex(mutex_t *p);

void mutex_lock(mutex_t *p);

void mutex_unlock(mutex_t *p);

#endif
