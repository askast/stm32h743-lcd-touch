#ifndef UART_H
#define UART_H

#include <stddef.h>

/* Configure USART1 on PA9 (TX) / PA10 (RX) for 115200 8N1, clocked from the
 * 64 MHz HSI (the reset-default system clock). */
void uart_init(void);

/* Blocking transmit of `len` bytes. */
void uart_write(const char *buf, size_t len);

#endif /* UART_H */
