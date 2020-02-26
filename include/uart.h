#ifndef __UART_H__
#define __UART_H__

void init_uart(void);
void start_uart_tx(void);
void handle_uart_irq(void);
void write_char_uart(char c);

#define UART_IRQ_MODE

#endif
