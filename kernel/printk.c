#include <hardware.h>
#include <printk.h>
#include <stddef.h>
#include <spinlock.h>
#include <uart.h>
#include <circular_buffer.h>
#include <misc.h>
#include <print.h>
#include <memory.h>
#include <percpu.h>

#define LOG_BUF_SIZE 0x1000000
static char *__log_buf;

#define TMP_PRINTK_BUF_LEN 1024
DEFINE_PER_CPU(char[TMP_PRINTK_BUF_LEN], tmp_printk_buf);
DEFINE_PER_CPU(int, tmp_printk_offset);

struct concurrent_cbuf kernel_log;

void init_printk(void)
{
	__log_buf = get_free_pages(get_order(LOG_BUF_SIZE));
	if (__log_buf == NULL) {
		return;
	}
	init_concurrent_cbuf(&kernel_log, __log_buf, LOG_BUF_SIZE);
}

#ifdef UART_IRQ_MODE
static int print_char(char c)
{
	int core_id = get_cpu_core_id();

	per_cpu(tmp_printk_buf, core_id)[per_cpu(tmp_printk_offset, core_id)] = c;

	per_cpu(tmp_printk_offset, core_id)++;
	if (per_cpu(tmp_printk_offset, core_id) >= TMP_PRINTK_BUF_LEN) {
		per_cpu(tmp_printk_offset, core_id) = 0;
	}
	return 1;
}
#else
static int print_char(char c)
{
	write_char_uart(c);

	return 1;
}
#endif

int printk(const char *fmt, ...)
{
	int num_printed = 0;
	va_list ap;
	int d;
	void *p;
	char c, *s;
	int width;
	char pad;
	int ret;
	struct timestamp cur_ts;
	int num_ts_printed = 0;
	int core_id = get_cpu_core_id();

	if (fmt == NULL) {
		return 0;
	}

	per_cpu(tmp_printk_offset, core_id) = 0;

	cur_ts = get_timestamp();
	num_ts_printed += print_string("[");
	num_ts_printed += print_unsigned_int(cur_ts.sec, 10, 5, ' ');
	num_ts_printed += print_string(".");
	num_ts_printed += print_unsigned_int(cur_ts.usec, 10, 6, '0');
	num_ts_printed += print_string("@cpu");
	num_ts_printed += print_int32_t(core_id);
	num_ts_printed += print_string("] ");

	PRINT

	ret = write_concurrent_cbuf(&kernel_log, per_cpu(tmp_printk_buf, core_id),
				    (num_ts_printed + num_printed));

	if (ret == 0) {
		return num_printed;
	} else {
		return -1;
	}
}

#define print_func printk

void test_printk(void)
{
	int ret_val;
	char test_string[] = "string test: Hello, world!";
	char test_int[] = "int test: ";
	char test_neg_int[] = "negative int test: ";
	char test_tab_char[] = "tab char test: ";
	char test_hex_int[] = "hex int test: ";
	char test_hex_neg_int[] = "hex neg int test: ";
	char test_octal_int[] = "octal int test: ";
	char test_octal_neg_int[] = "octal neg int test: ";
	char test_pointer[] = "pointer test: ";
	char test_neg_pointer[] = "neg pointer test: ";
	char test_unsigned_decimal[] = "unsigned decimal test: ";

	print_func("%s\n", "Testing print_func begin");
	ret_val = print_func("%s\n", test_string);
	assert(ret_val == sizeof (test_string) - 1 + 1);
	ret_val = print_func("%s%d\n", test_int, 123);
	assert(ret_val == sizeof (test_int) - 1 + 3 + 1);
	ret_val = print_func("%s%d\n", test_neg_int, -123);
	assert(ret_val == sizeof (test_neg_int) - 1 + 4 + 1);
	ret_val = print_func("%s\t%c\n", test_tab_char, 'a');
	assert(ret_val == sizeof (test_tab_char) - 1 + 1 + 1 + 1);
	ret_val = print_func("%s%x\n", test_hex_int, 0xFF);
	assert(ret_val == sizeof (test_hex_int) - 1 + 2 + 1);
	ret_val = print_func("%s%x\n", test_hex_neg_int, -1);
	assert(ret_val == sizeof (test_hex_neg_int) - 1 + 8 + 1);
	ret_val = print_func("%s%o\n", test_octal_int, 037);
	assert(ret_val == sizeof (test_octal_int) - 1 + 2 + 1);
	ret_val = print_func("%s%o\n", test_octal_neg_int, -1);
	assert(ret_val == sizeof (test_octal_neg_int) - 1 + 11 + 1);
	ret_val = print_func("%s%p\n", test_pointer, &test_pointer);
	assert(ret_val == sizeof (test_pointer) - 1 + NUM_HEX_FOR_PTR + 1);
	ret_val = print_func("%s%p\n", test_neg_pointer, -1LL);
	assert(ret_val == sizeof (test_neg_pointer) - 1 + NUM_HEX_FOR_PTR + 1);
	ret_val = print_func("%s%u\n", test_unsigned_decimal, -1);
	assert(ret_val == sizeof (test_unsigned_decimal) - 1 + 10 + 1);
	print_func("tab \t%s, %s, %d, %s, %s\n\r", __FILE__, __FUNCTION__,
			__LINE__, __DATE__, __TIME__);
	print_func("%s\n", "Testing print_func end");
}
