#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include "clock.h"
#include "sdram.h"
#include "lcd.h"
#include "touch.h"

/* Report faults over UART instead of silently parking (handy during bring-up). */
void HardFault_Handler(void)
{
    printf("\r\n!! HARDFAULT !!\r\n");
    for (;;) { }
}

int main(void)
{
    /* UART runs on the 64 MHz HSI default, so it survives the clock switch. */
    uart_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\r\n==== STM32H743 LCD bring-up ====\r\n");

    if (clock_init() != 0) {
        printf("[boot] clock_init FAILED -- halting.\r\n");
        for (;;) { }
    }
    sdram_init();   /* 32 MB FMC SDRAM @ 0xC0000000 holds the framebuffer. */

    /* Color-bar splash, then clear to a black canvas for the touch demo. */
    lcd_init();
    lcd_test_pattern();
    clock_delay_ms(1500);
    lcd_fill(0x0000);

    /* GT911 capacitive touch: drag a finger to draw white dots. */
    touch_init();
    printf("[boot] ready -- drag to draw.\r\n");

    for (;;) {
        uint16_t x, y;
        if (touch_read(&x, &y))
            lcd_draw_dot(x, y, 0xFFFF);
        clock_delay_ms(15);
    }
}
