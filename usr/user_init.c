#include <mmu.h>
#include <hardware.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <test_mem_alloc.h>

int test_data_value = 1;
int test_bss_value;

static int test_sbrk(void);
static void test_user_stack(void);
static int test_sbrk_unmap(void);
static void test_user_exit(void);
static void test_malloc_free(void);
static int test_user_exec_kernel(void);
static int test_user_read_kernel(void);
static int shell_main(void);

int init(void)
{
	int ret;

	printf("userspace init\n");

	ret = fork();

	if (ret > 0) {
		printf("In parent\n");
	} else if (ret == 0) {
		printf("In child\n");

		test_sbrk();
		_exit(0);
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	ret = fork();
	if (ret > 0) {
	} else if (ret == 0) {
		test_user_exec_kernel();
		_exit(0);
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	ret = fork();
	if (ret > 0) {
	} else if (ret == 0) {
		test_user_read_kernel();
		_exit(0);
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	ret = fork();
	if (ret > 0) {
	} else if (ret == 0) {
		test_sbrk_unmap();
		_exit(0);
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	ret = fork();
	if (ret > 0) {
	} else if (ret == 0) {
		test_user_stack();
		_exit(0);
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	ret = fork();
	if (ret > 0) {
	} else if (ret == 0) {
		test_user_exit();
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	ret = fork();
	if (ret > 0) {
	} else if (ret == 0) {
		init_malloc_free();
		test_malloc_free();
		_exit(-1);
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	ret = fork();
	if (ret > 0) {
	} else if (ret == 0) {
		int sleep_time = 1;
		for (int i = 0; i < 2; i++) {
			printf("Sleeping for %d seconds.\n", sleep_time);
			sleep(sleep_time);
		}
		_exit(-1);
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	ret = fork();
	if (ret > 0) {
	} else if (ret == 0) {
		while (true) {
			ret = shell_main();
			printf("shell exited, ret=%d\n", ret);
		}
	} else {
		printf("fork failed, ret=%d\n", ret);
		_exit(0);
	}

	while (true) {
		printf("Entering pause()\n");
		pause();
	}

	return test_data_value;
}

static int shell_main(void)
{
	static char *prompt = "# ";
	int ret;

	while (true) {
#define LINE_BUF_SIZE 256
		char buf[LINE_BUF_SIZE];
		size_t count = sizeof (buf);

		printf("%s", prompt);
		ret = read(0, buf, count);
		if (ret > 0) {
			count = ret;

			if (strncmp(buf, "help", 4) == 0 || strncmp(buf, "man", 3) == 0) {
				static char *help = "Built-in commands: help, man\n";
				ret = write(1, help, strlen(help));
			} else if (buf[0] != '\n') {
				static char *unknown_cmd = ": command not found\n";
				ret = write(1, buf, count - 1);
				ret = write(1, unknown_cmd, strlen(unknown_cmd));
			}
		}
	}

	return 0;
}

static void test_malloc_free(void)
{
	struct test_mem_alloc test_malloc_struct;
	int ret;

	printf("Testing malloc.\n");
	init_test_mem_alloc(&test_malloc_struct, MALLOC_BLK, malloc, free,
			    NULL, NULL);

	while (true) {
		ret = test_mem(&test_malloc_struct);
		if (ret < 0) {
			break;
		}
	}

	printf("malloc test over.\n");

	_exit(0);
}

static void test_user_exit(void)
{
	printf("%s, user process exiting\n", __FUNCTION__);
	_exit(-1);
}

static u64 factorial(u64 n)
{
	if (n == 0) {
		return 1;
	}

	return (n * factorial(n - 1));
}

static void test_user_stack(void)
{
	while (true) {
		factorial(1000);
	}
}

static int test_sbrk_unmap(void)
{
	void *oldbrk;
	void *newbrk;
	int increment = PAGE_SIZE * 3;

	oldbrk = sbrk(increment);
	newbrk = sbrk(0);
	if ((intptr_t)oldbrk + increment != (intptr_t)newbrk) {
		printf("Increase brk failed, increment=%u\n", increment);
	}
	memset(oldbrk, 0x5a, increment);
	oldbrk = sbrk(0-increment);
	newbrk = sbrk(0);
	if ((intptr_t)oldbrk - increment != (intptr_t)newbrk) {
		printf("Decrease brk failed, increment=%d\n", increment);
	}
	memset(newbrk, 0x5a, increment);
	printf("%s should not reach here\n", __FUNCTION__);

	return 0;
}

static int test_sbrk(void)
{
	void *oldbrk;
	void *newbrk;
	int increment;
	int failed = false;

	for (increment = 0; increment < PAGE_SIZE * 20; increment += 113) {
		oldbrk = sbrk(increment);
		newbrk = sbrk(0);
		if ((intptr_t)oldbrk + increment != (intptr_t)newbrk) {
			printf("Increase brk failed, increment=%u\n", increment);
			failed = true;
			break;
		}
		memset(oldbrk, 0x5a, increment);
		oldbrk = sbrk(0-increment);
		newbrk = sbrk(0);
		if ((intptr_t)oldbrk - increment != (intptr_t)newbrk) {
			printf("Decrease brk failed, increment=%d\n", increment);
			failed = true;
			break;
		}
	}

	if (failed) {
		printf("test sbrk failed\n");
	} else {
		printf("test sbrk success\n");
	}

	return 0;
}

static int test_user_exec_kernel(void)
{
	printf("%s\n", __FUNCTION__);
	asm volatile (
		      "ldr X0, =main\n\t"
		      "blr X0"
		      :
		     );
	printf("%s: should not reach here\n", __FUNCTION__);

	return 0;
}

static int test_user_read_kernel(void)
{
	printf("%s\n", __FUNCTION__);
	asm volatile (
		      "ldr X0, =main\n\t"
		      "ldr X1, [X0]"
		      :
		     );
	printf("%s: should not reach here\n", __FUNCTION__);

	return 0;
}
