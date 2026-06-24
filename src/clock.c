#include "clock.h"
#include "stm32h743_reg.h"

/*
 * Clock bring-up for the LCD/SDRAM work. See clock.h for the resulting tree.
 *
 * Design choice: the PLLs are driven from HSI (64 MHz), NOT the HSE crystal.
 * The board's crystal value is unconfirmed (and may be unpopulated), whereas
 * HSI is crystal-independent and already proven on this board. USART1 is left
 * on its HSI kernel clock so the existing 115200 baud math (UART_CLK_HZ =
 * 64 MHz) keeps working unchanged across the clock switch.
 *
 * Every "wait for ready" spin is bounded so a misconfiguration reports a step
 * code over UART instead of hanging the chip with no output.
 */

#define WAIT_LIMIT 2000000U

/* Spin until (expr) is true or the timeout expires. Returns 1 on timeout. */
#define WAIT_FOR(expr) ({                       \
    uint32_t _n = WAIT_LIMIT;                   \
    while (!(expr) && _n) { _n--; }             \
    (_n == 0);                                  \
})

void clock_delay_ms(uint32_t ms)
{
    /* ~400 MHz core. Each loop iteration is a few cycles; 100000 iters/ms is a
     * deliberate over-estimate so SDRAM power-up minimums are comfortably met. */
    for (volatile uint32_t i = 0; i < ms * 100000U; i++) {
        __asm volatile("nop");
    }
}

int clock_init(void)
{
    /* Keep USART1 on the HSI kernel clock for the whole sequence so the UART
     * baud math (64 MHz) is valid before, during, and after the bus switch. */
    RCC_D2CCIP2R = (RCC_D2CCIP2R & ~RCC_USART16SEL_Msk) | RCC_USART16SEL_HSI;

    /* 1. Finalize the power supply, then raise the core voltage to Scale 1
     *    (required for 400 MHz). The board uses the internal LDO (reset default
     *    LDOEN=1); SCUEN must be cleared to finalize the supply config or
     *    VOSRDY never asserts. Wait ACTVOSRDY, then select Scale 1 and wait. */
    PWR_CR3 &= ~PWR_CR3_SCUEN;
    if (WAIT_FOR(PWR_CSR1 & PWR_CSR1_ACTVOSRDY)) return 1;
    PWR_D3CR |= PWR_D3CR_VOS_SCALE1;
    if (WAIT_FOR(PWR_D3CR & PWR_D3CR_VOSRDY)) return 1;

    /* 2. HSI is already on (SystemInit forced 64 MHz); make sure. */
    RCC_CR |= RCC_CR_HSION;
    if (WAIT_FOR(RCC_CR & RCC_CR_HSIRDY)) return 2;

    /* 3. PLL source = HSI, DIVM1 = 16 (-> 4 MHz), DIVM3 = 16 (-> 4 MHz). */
    RCC_PLLCKSELR = RCC_PLLSRC_HSI
                  | (16U << RCC_DIVM1_Pos)
                  | (16U << RCC_DIVM3_Pos);

    /* 4. PLL input ranges = 4-8 MHz (RGE=10b), wide VCO (VCOSEL=0),
     *    enable PLL1-P (SYSCLK) and PLL3-R (LTDC). */
    RCC_PLLCFGR = (2U << RCC_PLL1RGE_Pos)
                | (2U << RCC_PLL3RGE_Pos)
                | RCC_DIVP1EN
                | RCC_DIVR3EN;

    /* 5. PLL1: N=200, P=2  -> VCO 800 MHz, SYSCLK 400 MHz. (fields are value-1) */
    RCC_PLL1DIVR = ((200U - 1U) << 0)   /* DIVN1 */
                 | ((2U  - 1U) << 9);   /* DIVP1 */

    /* 6. PLL3: N=50, R=8 -> VCO 200 MHz, LTDC pixel clock 25 MHz (the value the
     *    board's panel actually uses; 4 MHz * 50 / 8 = 25 MHz). */
    RCC_PLL3DIVR = ((50U - 1U) << 0)    /* DIVN3 */
                 | ((8U  - 1U) << 24);  /* DIVR3 */

    /* 7. Enable PLL1 and PLL3, wait for lock. */
    RCC_CR |= RCC_CR_PLL1ON;
    if (WAIT_FOR(RCC_CR & RCC_CR_PLL1RDY)) return 7;
    RCC_CR |= RCC_CR_PLL3ON;
    if (WAIT_FOR(RCC_CR & RCC_CR_PLL3RDY)) return 8;

    /* 8. Flash latency for 200 MHz AXI at VOS1: 2 wait states. Set BEFORE the
     *    switch to the faster clock. */
    FLASH_ACR = FLASH_ACR_LATENCY_2 | FLASH_ACR_WRHIGHFREQ_2;

    /* 9. Bus prescalers: HPRE /2 (HCLK 200), all PPRE /2. D1CPRE /1. */
    RCC_D1CFGR = (8U << 0)    /* HPRE  = /2 */
               | (4U << 4)    /* D1PPRE= /2 */
               | (0U << 8);   /* D1CPRE= /1 */
    RCC_D2CFGR = (4U << 4)    /* D2PPRE1 = /2 */
               | (4U << 8);   /* D2PPRE2 = /2 */
    RCC_D3CFGR = (4U << 4);   /* D3PPRE  = /2 */

    /* 10. Switch SYSCLK to PLL1 and wait until it is the active source. */
    RCC_CFGR = (RCC_CFGR & ~RCC_CFGR_SWS_Msk) | RCC_CFGR_SW_PLL1;
    if (WAIT_FOR((RCC_CFGR & RCC_CFGR_SWS_Msk) == RCC_CFGR_SWS_PLL1)) return 10;

    return 0;
}
