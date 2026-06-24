#include "uart.h"
#include "stm32h743_reg.h"

/* USART1 kernel clock = APB2 clock = 64 MHz (HSI, reset default). */
#define UART_CLK_HZ 64000000UL
#define UART_BAUD   115200UL

void uart_init(void)
{
    /* Enable clocks to GPIOA and USART1, then read back to ensure the clock is
     * running before the peripheral registers are configured. */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC_AHB4ENR;

    /* PA9, PA10 -> alternate-function mode (MODER = 0b10). */
    GPIOA_MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA_MODER |=  ((2U << (9 * 2)) | (2U << (10 * 2)));

    /* Very-high output speed for the TX/RX pins. */
    GPIOA_OSPEEDR |= (3U << (9 * 2)) | (3U << (10 * 2));

    /* AF7 = USART1 for PA9/PA10 (pins 8..15 live in AFRH, 4 bits each). */
    GPIOA_AFRH &= ~((0xFU << ((9 - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA_AFRH |=  ((7U   << ((9 - 8) * 4)) | (7U   << ((10 - 8) * 4)));

    /* Disable while configuring; set baud; enable TX, RX, and the USART. */
    USART1_CR1 = 0;
    USART1_BRR = (UART_CLK_HZ + UART_BAUD / 2) / UART_BAUD; /* rounded */
    USART1_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void uart_write(const char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        while (!(USART1_ISR & USART_ISR_TXE)) { /* wait for TDR empty */ }
        USART1_TDR = (uint8_t)buf[i];
    }
    while (!(USART1_ISR & USART_ISR_TC)) { /* wait for last byte to leave */ }
}
