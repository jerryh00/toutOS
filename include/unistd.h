#ifndef _UNISTD_H
#define _UNISTD_H

#include <time.h>

#define __NR_fork "0"
#define __NR_brk "1"
#define __NR_exit "2"
#define __NR_nanosleep "3"
#define __NR_pause "4"
#define __NR_read "5"
#define __NR_write "6"

typedef long pid_t;

pid_t fork(void);
int brk(void *addr);
void *sbrk(intptr_t increment);
void _exit(int status);

int nanosleep(const struct timespec *req, struct timespec *rem);
unsigned int sleep(unsigned int seconds);
int pause(void);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

#endif
