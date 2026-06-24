#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/*
 * Bring the system up to 400 MHz from the on-chip HSI (no external crystal
 * needed), enabling PLL3-R for the LTDC pixel clock and keeping USART1 on HSI.
 *
 * Resulting clock tree:
 *   SYSCLK = 400 MHz   (PLL1-P, HSI 64 MHz / 16 * 200 / 2)
 *   HCLK   = 200 MHz   (AHB / 2)        -> FMC kernel clock
 *   SDRAM  = 100 MHz   (HCLK / 2, set in the FMC SDCR)
 *   PLL3-R =  25 MHz   (LTDC pixel clock)
 *   USART1 =  64 MHz   (HSI, unchanged -> UART_CLK_HZ stays valid)
 *
 * Returns 0 on success, or a non-zero step code if a status bit never came
 * ready (so the caller can report it over an already-initialized UART rather
 * than hanging forever).
 */
int clock_init(void);

/* Crude blocking delay, generously over-estimated (for peripheral power-up). */
void clock_delay_ms(uint32_t ms);

#endif /* CLOCK_H */
