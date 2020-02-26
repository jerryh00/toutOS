#ifndef _TIME_H
#define _TIME_H

typedef long time_t;

struct timespec {
	time_t tv_sec;        /* seconds */
	long   tv_nsec;       /* nanoseconds */
};

#endif
