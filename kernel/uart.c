#include <arch.h>
#include <uart.h>
#include <hardware.h>
#include <circular_buffer.h>
#include <printk.h>
#include <mmu.h>
#include <sched.h>
#include <percpu.h>
#include <hw_timer.h>
#include <wait.h>

extern struct concurrent_cbuf kernel_log;

struct wait_queue_head uib_wq_head;

#define UART_IN_BUF_LEN 256
uint8_t uart_in_buf[UART_IN_BUF_LEN];
int uib_index;
struct spinlock uib_lock;

int uart_busy = false;
static int uart_fifo_size = 16;
static int uart_tx_fifo_one_write_len = 14;

static void config_uart(void)
{
	*(unsigned int *)__va(UART_UARTCR) = 0;	/* Disable uart */
	*(unsigned int *)__va(UART_UARTIBRD) = 0x0D;	/* Set baudrate */
	*(unsigned int *)__va(UART_UARTFBRD) = 0x02;
#ifdef UART_IRQ_MODE
	*(unsigned int *)__va(UART_UARTIMSC) |= (1<<4);	/* Receive interrupt */
	*(unsigned int *)__va(UART_UARTIMSC) |= (1<<5);	/* Transmit interrupt */
	*(char*)__va(UART_UARTLCR_H) |= (1<<4);	/* Enable FIFO */
	*(char*)__va(UART_UARTIFLS) = 0;	/* FIFO level set:
						   Receive FIFO becomes >= 1/8 full,
						   Transmit FIFO becomes <= 1/8 full */
#endif
#ifdef DEBUG_UART
	printk("uart interrupt FIFO level select %p\n", *(char*)__va(UART_UARTIFLS));
	printk("uart line control %p\n", *(char*)__va(UART_UARTLCR_H));
#endif
	*(unsigned int *)__va(UART_UARTCR) = 0x0301;	/* Enable uart */
}

void init_uart(void)
{
	uint32_t uart_periph_id2 = *((uint32_t *)__va(UART_UARTPeriphID2));
	uart_fifo_size = (((uart_periph_id2 & 0xFF)>> 4) < 0x3) ? 16 : 32;
	printk("uart_periph_id2=0x%x, uart_fifo_size=%d\n", uart_periph_id2, uart_fifo_size);
	uart_tx_fifo_one_write_len = (uart_fifo_size * 7 / 8);

	config_uart();

	init_waitqueue_head(&uib_wq_head);
	spin_lock_init(&uib_lock);
}

void start_uart_tx(void)
{
	int ret;
	char c;

	if (!uart_busy) {
		ret = read_concurrent_cbuf(&kernel_log, &c, 1);
		if (ret == 1) {
			write_char_uart(c);
			uart_busy = true;
		}
	}
}

static void handle_magic_keys(char c)
{
	static int is_in_magic = false;

	if (is_in_magic) {
		if (c == 'g') {
			DECLARE_PER_CPU(uint64_t[MAX_NUM_INTERRUPTS], irq_trigger_count);
			int core_id = get_cpu_core_id();
			printk("kernel buf write overflow"
			       "(non-zero means missing logs): %d\n",
			       kernel_log.write_overflow);
			printk("tick=%d\n", get_tick());
			printk("uart irq_trigger_count@cpu%d=%d\n", core_id, per_cpu(irq_trigger_count, core_id)[IRQ_UART]);
			printk("timer irq_trigger_count@cpu%d=%d\n", core_id, per_cpu(irq_trigger_count, core_id)[IRQ_TIMER]);
		}
		if (c == 'p') {
			dump_tasks();
		}
	}

	if (c == 0x19) { /* ctrl-y */
		is_in_magic = true;
	} else {
		is_in_magic = false;
	}
}

static inline void cursor_backward(uint8_t count)
{
	write_char_uart(0x1B);
	write_char_uart(0x5B);
	write_char_uart(count);
	write_char_uart('D');
}

static void to_line_discipline(char c)
{
	unsigned long flags;

	flags = spin_lock_irqsave(&uib_lock);
	if (c == '\b' || c == 0x7F) { /* backspace/DEL */
		if (uib_index > 0) {
			uib_index--;

			cursor_backward(1);
			write_char_uart(' ');
			cursor_backward(1);
		}

		spin_unlock_irqrestore(&uib_lock, flags);
		return;
	}

	if (c == '\r') { /* QEMU sends '\r' for enter key. */
		c = '\n';
	}

	if (uib_index < UART_IN_BUF_LEN - 1) {
		uart_in_buf[uib_index] = c;
		uib_index++;
	}
	spin_unlock_irqrestore(&uib_lock, flags);

	if (c == '\n') {
		wake_up(&uib_wq_head);
	}

	write_char_uart(c);
}

void handle_uart_irq(void)
{
	char c;
	char read_buf[32];
	int ret;

	if (((*(uint32_t *)__va(UART_UARTMIS)) & (1<<4)) == (1<<4)) {
		c = *(uint32_t *)__va(UART_UARTDR);
		handle_magic_keys(c);
		to_line_discipline(c);
	}
	if (((*(uint32_t *)__va(UART_UARTMIS)) & (1<<5)) == (1<<5)) {
		ret = read_concurrent_cbuf(&kernel_log, read_buf, uart_tx_fifo_one_write_len);

		if (ret > 0) {
			for (int i = 0; i < ret; i++) {
				write_char_uart(read_buf[i]);
			}
		} else {
			uart_busy = false;
			*((uint32_t *)__va(UART_UARTICR)) = (1<<5);
		}
	}
}

void write_char_uart(char c)
{
	*(uint32_t *)__va(UART_UARTDR) = c;
}
