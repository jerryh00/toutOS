#include <unistd.h>

pid_t fork(void)
{
	long __res;

	asm volatile (
		"mov X8, "__NR_fork"\n\t"
		"svc #0\n\t"
		"mov %0, X0\n\t"
		: "=r" (__res));

	return __res;
}

static void *_sys_brk(void *addr)
{
	long __res;

	asm volatile (
		"mov X8, "__NR_brk"\n\t"
		"mov X0, %1\n\t"
		"svc #0\n\t"
		"mov %0, X0\n\t"
		: "=r" (__res)
		: "r" (addr));

	return (void *)__res;
}

int brk(void *addr)
{
	void *ret;

	if (addr == NULL) {
		return -1;
	}

	ret = _sys_brk(addr);

	if (ret != addr) {
		return -1;
	}

	return 0;
}

void *sbrk(intptr_t increment)
{
	void *ret;
	void *old_brk;
	void *new_brk;

	old_brk = _sys_brk(0);
	if (increment == 0) {
		return old_brk;
	}

	new_brk = (char *)old_brk + increment;

	ret = _sys_brk(new_brk);
	if (ret != new_brk) {
		return (void *)-1;
	}

	return old_brk;
}

void _exit(int status)
{
	asm volatile (
		"mov X8, "__NR_exit"\n\t"
		"mov X0, %0\n\t"
		"svc #0\n\t"
		:
		: "r" (status));
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	long __res;

	if (req == NULL || req->tv_sec < 0 ||
	    req->tv_nsec < 0 || req->tv_nsec >= (int)1e9) {
		return -1;
	}

	asm volatile (
		"mov X8, "__NR_nanosleep"\n\t"
		"mov X0, %1\n\t"
		"mov X1, %2\n\t"
		"svc #0\n\t"
		"mov %0, X0\n\t"
		: "=r" (__res)
		: "r" (req), "r" (rem));

	return __res;
}

unsigned int sleep(unsigned int seconds)
{
	struct timespec req = {0, 0};
	struct timespec rem;
	int ret;

	req.tv_sec += seconds;
	ret = nanosleep(&req, &rem);
	if (ret == 0) {
		return 0;
	} else {
		return rem.tv_sec + ((rem.tv_nsec != 0) ? 1 : 0);
	}
}

int pause(void)
{
	long __res;

	asm volatile (
		"mov X8, "__NR_pause"\n\t"
		"svc #0\n\t"
		"mov %0, X0\n\t"
		: "=r" (__res));

	return __res;
}

ssize_t read(int fd, void *buf, size_t count)
{
	long __res;

	if (fd < 0 || buf == NULL) {
		return -1;
	}

	asm volatile (
		"mov X8, "__NR_read"\n\t"
		"mov X0, %1\n\t"
		"mov X1, %2\n\t"
		"mov X2, %3\n\t"
		"svc #0\n\t"
		"mov %0, X0\n\t"
		: "=r" (__res)
		: "r" (fd), "r" (buf), "r" (count));

	return __res;
}

ssize_t write(int fd, const void *buf, size_t count)
{
	long __res;

	if (fd < 0 || buf == NULL) {
		return -1;
	}

	asm volatile (
		"mov X8, "__NR_write"\n\t"
		"mov X0, %1\n\t"
		"mov X1, %2\n\t"
		"mov X2, %3\n\t"
		"svc #0\n\t"
		"mov %0, X0\n\t"
		: "=r" (__res)
		: "r" (fd), "r" (buf), "r" (count));

	return __res;
}
