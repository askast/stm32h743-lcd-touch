/*
 * Clock diagnostic firmware (not part of the normal app).
 *
 * Build:  cmake --build build --target clock-report
 * Flash:  STM32_Programmer_CLI -c port=USB1 -w build/clock-report.hex
 *         then RESET with BOOT0 low.
 *
 * It runs the project's real clock_init() and then reports the resulting clock
 * tree two independent ways, looping once a second so a serial listener can
 * catch it at any time (USART1, 115200 8N1, PA9/PA10):
 *
 *   1. CONFIG  -- decode the PLL/prescaler registers actually in effect and
 *      compute SYSCLK / HCLK / PLL3-R from them + the known 64 MHz HSI. This is
 *      authoritative for the divider configuration (settles the PLL3-R value).
 *
 *   2. MEASURED -- time the DWT cycle counter (which runs at the CPU/SYSCLK
 *      rate) across the transmission of a fixed number of UART bytes. The UART
 *      bit clock comes from HSI (64 MHz), independent of SYSCLK, so this is a
 *      genuine physical cross-check that the PLL multiplied as configured.
 */
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include "clock.h"
#include "stm32h743_reg.h"

#define HSI_HZ 64000000UL

/* Cortex-M7 DWT cycle counter (not in the project's minimal reg header). */
#define DEMCR      REG32(0xE000EDFCUL)
#define DWT_CTRL   REG32(0xE0001000UL)
#define DWT_CYCCNT REG32(0xE0001004UL)
#define DEMCR_TRCENA   (1U << 24)
#define DWT_CYCCNTENA  (1U << 0)

/* Decode a 4-bit HPRE/D1CPRE prescaler field to its divide value. */
static uint32_t pre_div(uint32_t field)
{
    static const uint32_t d[8] = { 2, 4, 8, 16, 64, 128, 256, 512 };
    return (field < 8U) ? 1U : d[field - 8U];
}

void HardFault_Handler(void)
{
    printf("\r\n!! HARDFAULT in clock-report !!\r\n");
    for (;;) { }
}

int main(void)
{
    uart_init();
    setvbuf(stdout, NULL, _IONBF, 0);

    int rc = clock_init();

    /* ---- (1) decode the registers actually in effect ---- */
    uint32_t cksel = RCC_PLLCKSELR;
    uint32_t divm1 = (cksel >> RCC_DIVM1_Pos) & 0x3FU;
    uint32_t divm3 = (cksel >> RCC_DIVM3_Pos) & 0x3FU;

    uint32_t p1 = RCC_PLL1DIVR;
    uint32_t divn1 = ((p1 >> 0)  & 0x1FFU) + 1U;   /* 9-bit, stored value-1  */
    uint32_t divp1 = ((p1 >> 9)  & 0x7FU)  + 1U;   /* 7-bit, stored value-1  */

    uint32_t p3 = RCC_PLL3DIVR;
    uint32_t divn3 = ((p3 >> 0)  & 0x1FFU) + 1U;
    uint32_t divr3 = ((p3 >> 24) & 0x7FU)  + 1U;

    uint32_t d1 = RCC_D1CFGR;
    uint32_t hpre   = pre_div((d1 >> 0) & 0xFU);
    uint32_t d1cpre = pre_div((d1 >> 8) & 0xFU);

    uint32_t pll1_ref = HSI_HZ / divm1;
    uint32_t vco1     = pll1_ref * divn1;
    uint32_t pll1p    = vco1 / divp1;
    uint32_t sysclk   = pll1p / d1cpre;
    uint32_t hclk     = sysclk / hpre;

    uint32_t pll3_ref = HSI_HZ / divm3;
    uint32_t vco3     = pll3_ref * divn3;
    uint32_t pll3r    = vco3 / divr3;

    uint32_t sws = (RCC_CFGR >> 3) & 0x7U;         /* active SYSCLK source   */
    const char *src = (sws == 3U) ? "PLL1" : (sws == 2U) ? "HSE" :
                      (sws == 1U) ? "CSI"  : "HSI";

    /* ---- (2) independent measurement: DWT cycles per UART byte burst ---- */
    DEMCR |= DEMCR_TRCENA;
    DWT_CTRL |= DWT_CYCCNTENA;

    const uint32_t N = 2000U;                      /* 2000 bytes ~ 173 ms    */
    DWT_CYCCNT = 0U;
    uint32_t c0 = DWT_CYCCNT;
    for (uint32_t i = 0; i < N; i++) {
        while (!(USART1_ISR & USART_ISR_TXE)) { }
        USART1_TDR = (uint8_t)'.';
    }
    while (!(USART1_ISR & USART_ISR_TC)) { }       /* last byte fully out    */
    uint32_t cycles = DWT_CYCCNT - c0;
    /* N bytes * 10 bits each at 115200 baud (HSI-timed) = elapsed seconds.
     * SYSCLK = cycles / elapsed = cycles * 115200 / (N * 10). */
    uint32_t sysclk_meas = (uint32_t)((uint64_t)cycles * 115200ULL / ((uint64_t)N * 10ULL));

    for (;;) {
        printf("\r\n==== STM32H743 clock report ====\r\n");
        printf("clock_init() rc = %d   SYSCLK source = %s\r\n", rc, src);
        printf("PLL1: M=%lu N=%lu P=%lu  VCO=%lu Hz\r\n",
               (unsigned long)divm1, (unsigned long)divn1, (unsigned long)divp1,
               (unsigned long)vco1);
        printf("PLL3: M=%lu N=%lu R=%lu  VCO=%lu Hz\r\n",
               (unsigned long)divm3, (unsigned long)divn3, (unsigned long)divr3,
               (unsigned long)vco3);
        printf("[config ] SYSCLK = %lu Hz (%lu MHz)\r\n",
               (unsigned long)sysclk, (unsigned long)(sysclk / 1000000UL));
        printf("[config ] HCLK   = %lu Hz (%lu MHz)\r\n",
               (unsigned long)hclk, (unsigned long)(hclk / 1000000UL));
        printf("[config ] PLL3-R = %lu Hz (%lu MHz)  <- LTDC pixel clock\r\n",
               (unsigned long)pll3r, (unsigned long)(pll3r / 1000000UL));
        printf("[measure] SYSCLK = %lu Hz (%lu MHz)  via %lu DWT cycles / %lu bytes\r\n",
               (unsigned long)sysclk_meas, (unsigned long)(sysclk_meas / 1000000UL),
               (unsigned long)cycles, (unsigned long)N);
        clock_delay_ms(1000);
    }
}
