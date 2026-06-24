/*
 * Minimal register definitions for STM32H743 (Cortex-M7).
 *
 * This is NOT a full CMSIS device header. It defines only the peripheral
 * registers this project touches. Originally just RCC/GPIOA/USART1 for the
 * hello-world UART; now extended with the clock tree (PWR/FLASH/RCC PLLs),
 * all GPIO ports, the FMC SDRAM controller and the LTDC for the LCD bring-up.
 *
 * If you grow the project further, replace this with the official CMSIS
 * headers from STM32CubeH7 (stm32h743xx.h).
 *
 * Register offsets taken from RM0433 (STM32H742/743/753 reference manual).
 */
#ifndef STM32H743_REG_H
#define STM32H743_REG_H

#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* --- RCC (Reset and Clock Control) @ 0x58024400 --------------------------- */
#define RCC_BASE      0x58024400UL
#define RCC_CR        REG32(RCC_BASE + 0x00) /* clock control                */
#define RCC_CFGR      REG32(RCC_BASE + 0x10) /* clock config (SW/SWS)        */
#define RCC_D1CFGR    REG32(RCC_BASE + 0x18) /* domain 1 prescalers          */
#define RCC_D2CFGR    REG32(RCC_BASE + 0x14) /* domain 2 prescalers          */
#define RCC_D3CFGR    REG32(RCC_BASE + 0x1C) /* domain 3 prescalers          */
#define RCC_PLLCKSELR REG32(RCC_BASE + 0x28) /* PLL source + DIVM            */
#define RCC_PLLCFGR   REG32(RCC_BASE + 0x2C) /* PLL ranges + DIVxEN          */
#define RCC_PLL1DIVR  REG32(RCC_BASE + 0x30) /* PLL1 N/P/Q/R                 */
#define RCC_PLL3DIVR  REG32(RCC_BASE + 0x40) /* PLL3 N/P/Q/R                 */
#define RCC_D1CCIPR   REG32(RCC_BASE + 0x4C) /* domain 1 kernel clk mux      */
#define RCC_D2CCIP2R  REG32(RCC_BASE + 0x54) /* domain 2 kernel clk mux #2   */
#define RCC_AHB3ENR   REG32(RCC_BASE + 0xD4) /* AHB3 peripheral clock en     */
#define RCC_AHB4ENR   REG32(RCC_BASE + 0xE0) /* AHB4 peripheral clock en     */
#define RCC_APB2ENR   REG32(RCC_BASE + 0xF0) /* APB2 peripheral clock en     */
#define RCC_APB3ENR   REG32(RCC_BASE + 0xE4) /* APB3 peripheral clock en (LTDC) */
#define RCC_APB4ENR   REG32(RCC_BASE + 0xF4) /* APB4 peripheral clock en (I2C4) */
#define RCC_D3CCIPR   REG32(RCC_BASE + 0x58) /* D3 kernel clock mux          */
#define RCC_APB4ENR_I2C4EN  (1U << 7)

#define RCC_CR_HSION      (1U << 0)
#define RCC_CR_HSIRDY     (1U << 2)
#define RCC_CR_HSIDIV_Msk (3U << 3)          /* 00 = /1 -> 64 MHz            */
#define RCC_CR_HSEON      (1U << 16)
#define RCC_CR_HSERDY     (1U << 17)
#define RCC_CR_PLL1ON     (1U << 24)
#define RCC_CR_PLL1RDY    (1U << 25)
#define RCC_CR_PLL3ON     (1U << 28)
#define RCC_CR_PLL3RDY    (1U << 29)

/* PLLCKSELR fields */
#define RCC_PLLSRC_HSI    (0U << 0)
#define RCC_DIVM1_Pos     4
#define RCC_DIVM3_Pos     20
/* PLLCFGR fields */
#define RCC_PLL1VCOSEL    (1U << 1)   /* 1 = medium VCO (150-420 MHz)        */
#define RCC_PLL1RGE_Pos   2           /* 10b = 4-8 MHz input range           */
#define RCC_PLL3VCOSEL    (1U << 9)
#define RCC_PLL3RGE_Pos   10
#define RCC_DIVP1EN       (1U << 16)
#define RCC_DIVQ1EN       (1U << 17)
#define RCC_DIVR1EN       (1U << 18)
#define RCC_DIVP3EN       (1U << 22)
#define RCC_DIVQ3EN       (1U << 23)
#define RCC_DIVR3EN       (1U << 24)

/* CFGR fields */
#define RCC_CFGR_SW_PLL1  (3U << 0)
#define RCC_CFGR_SWS_Msk  (7U << 3)
#define RCC_CFGR_SWS_PLL1 (3U << 3)

/* GPIO/peripheral clock-enable bits */
#define RCC_AHB4ENR_GPIOAEN (1U << 0)
#define RCC_AHB4ENR_GPIOBEN (1U << 1)
#define RCC_AHB4ENR_GPIOCEN (1U << 2)
#define RCC_AHB4ENR_GPIODEN (1U << 3)
#define RCC_AHB4ENR_GPIOEEN (1U << 4)
#define RCC_AHB4ENR_GPIOFEN (1U << 5)
#define RCC_AHB4ENR_GPIOGEN (1U << 6)
#define RCC_AHB4ENR_GPIOHEN (1U << 7)
#define RCC_AHB4ENR_GPIOIEN (1U << 8)
#define RCC_AHB4ENR_GPIOJEN (1U << 9)
#define RCC_AHB4ENR_GPIOKEN (1U << 10)
#define RCC_AHB3ENR_FMCEN   (1U << 12)
#define RCC_APB3ENR_LTDCEN  (1U << 3)
#define RCC_APB2ENR_USART1EN (1U << 4)

/* D2CCIP2R: USART16SEL[5:3], 011 = HSI kernel clock */
#define RCC_USART16SEL_Pos  3
#define RCC_USART16SEL_HSI  (3U << RCC_USART16SEL_Pos)
#define RCC_USART16SEL_Msk  (7U << RCC_USART16SEL_Pos)

/* --- PWR @ 0x58024800 ----------------------------------------------------- */
#define PWR_BASE      0x58024800UL
#define PWR_CSR1      REG32(PWR_BASE + 0x04)
#define PWR_CR3       REG32(PWR_BASE + 0x0C)
#define PWR_D3CR      REG32(PWR_BASE + 0x18)
#define PWR_CR3_BYPASS      (1U << 0)
#define PWR_CR3_LDOEN       (1U << 1)
#define PWR_CR3_SCUEN       (1U << 2)  /* supply config update enable          */
#define PWR_CSR1_ACTVOSRDY  (1U << 13) /* actual VOS ready                     */
#define PWR_D3CR_VOS_SCALE1 (3U << 14)
#define PWR_D3CR_VOSRDY     (1U << 13)

/* --- FLASH @ 0x52002000 --------------------------------------------------- */
#define FLASH_BASE_REG 0x52002000UL
#define FLASH_ACR      REG32(FLASH_BASE_REG + 0x00)
#define FLASH_ACR_LATENCY_2    (2U << 0)
#define FLASH_ACR_WRHIGHFREQ_2 (2U << 4)

/* --- GPIO ports (each 0x400 apart) ---------------------------------------- */
#define GPIOA_BASE    0x58020000UL
#define GPIOB_BASE    0x58020400UL
#define GPIOC_BASE    0x58020800UL
#define GPIOD_BASE    0x58020C00UL
#define GPIOE_BASE    0x58021000UL
#define GPIOF_BASE    0x58021400UL
#define GPIOG_BASE    0x58021800UL
#define GPIOH_BASE    0x58021C00UL
#define GPIOI_BASE    0x58022000UL
#define GPIOJ_BASE    0x58022400UL
#define GPIOK_BASE    0x58022800UL

/* Generic GPIO register access (pass a *_BASE). */
#define GPIO_MODER(b)   REG32((b) + 0x00)
#define GPIO_OTYPER(b)  REG32((b) + 0x04)
#define GPIO_OSPEEDR(b) REG32((b) + 0x08)
#define GPIO_PUPDR(b)   REG32((b) + 0x0C)
#define GPIO_ODR(b)     REG32((b) + 0x14)
#define GPIO_BSRR(b)    REG32((b) + 0x18)
#define GPIO_AFRL(b)    REG32((b) + 0x20)
#define GPIO_AFRH(b)    REG32((b) + 0x24)

/* Legacy per-pin macros kept for uart.c. */
#define GPIOA_MODER   GPIO_MODER(GPIOA_BASE)
#define GPIOA_OSPEEDR GPIO_OSPEEDR(GPIOA_BASE)
#define GPIOA_AFRL    GPIO_AFRL(GPIOA_BASE)
#define GPIOA_AFRH    GPIO_AFRH(GPIOA_BASE)

/* --- USART1 @ 0x40011000 (APB2) ------------------------------------------- */
#define USART1_BASE   0x40011000UL
#define USART1_CR1    REG32(USART1_BASE + 0x00)
#define USART1_BRR    REG32(USART1_BASE + 0x0C)
#define USART1_ISR    REG32(USART1_BASE + 0x1C)
#define USART1_TDR    REG32(USART1_BASE + 0x28)

#define USART_CR1_UE  (1U << 0)
#define USART_CR1_RE  (1U << 2)
#define USART_CR1_TE  (1U << 3)
#define USART_ISR_TXE (1U << 7)  /* transmit data register empty */
#define USART_ISR_TC  (1U << 6)  /* transmission complete        */

/* --- FMC SDRAM controller @ 0x52004000 ------------------------------------ */
#define FMC_BASE      0x52004000UL
#define FMC_BCR1      REG32(FMC_BASE + 0x000) /* NOR/PSRAM ctrl 1; holds FMCEN */
#define FMC_BCR1_FMCEN (1U << 31)             /* global FMC enable (H7-only!)  */
#define FMC_SDCR1     REG32(FMC_BASE + 0x140) /* control bank 1 (common bits) */
#define FMC_SDCR2     REG32(FMC_BASE + 0x144) /* control bank 2               */
#define FMC_SDTR1     REG32(FMC_BASE + 0x148) /* timing bank 1 (common bits)  */
#define FMC_SDTR2     REG32(FMC_BASE + 0x14C) /* timing bank 2                */
#define FMC_SDCMR     REG32(FMC_BASE + 0x150) /* command mode                 */
#define FMC_SDRTR     REG32(FMC_BASE + 0x154) /* refresh timer                */
#define FMC_SDSR      REG32(FMC_BASE + 0x158) /* status                       */
#define FMC_SDSR_BUSY (1U << 5)

/* SDRAM bank base addresses. This board wires the SDRAM to FMC bank 1. */
#define SDRAM_BANK1_BASE 0xC0000000UL
#define SDRAM_BANK2_BASE 0xD0000000UL

/* --- LTDC (LCD-TFT controller) @ 0x50001000 ------------------------------- */
#define LTDC_BASE     0x50001000UL
#define LTDC_SSCR     REG32(LTDC_BASE + 0x08) /* sync size                    */
#define LTDC_BPCR     REG32(LTDC_BASE + 0x0C) /* back porch (accumulated)     */
#define LTDC_AWCR     REG32(LTDC_BASE + 0x10) /* active width (accumulated)   */
#define LTDC_TWCR     REG32(LTDC_BASE + 0x14) /* total width                  */
#define LTDC_GCR      REG32(LTDC_BASE + 0x18) /* global control               */
#define LTDC_SRCR     REG32(LTDC_BASE + 0x24) /* shadow reload                */
#define LTDC_BCCR     REG32(LTDC_BASE + 0x2C) /* background color             */
#define LTDC_CPSR     REG32(LTDC_BASE + 0x44) /* current position (X<<16|Y)   */
#define LTDC_CDSR     REG32(LTDC_BASE + 0x48) /* current display status       */
/* Layer 1 registers (base + 0x84). */
#define LTDC_L1CR     REG32(LTDC_BASE + 0x84)
#define LTDC_L1WHPCR  REG32(LTDC_BASE + 0x88)
#define LTDC_L1WVPCR  REG32(LTDC_BASE + 0x8C)
#define LTDC_L1PFCR   REG32(LTDC_BASE + 0x94)
#define LTDC_L1CACR   REG32(LTDC_BASE + 0x98)
#define LTDC_L1DCCR   REG32(LTDC_BASE + 0x9C)
#define LTDC_L1BFCR   REG32(LTDC_BASE + 0xA0)
#define LTDC_L1CFBAR  REG32(LTDC_BASE + 0xAC)
#define LTDC_L1CFBLR  REG32(LTDC_BASE + 0xB0)
#define LTDC_L1CFBLNR REG32(LTDC_BASE + 0xB4)

#define LTDC_GCR_LTDCEN (1U << 0)
#define LTDC_GCR_PCPOL  (1U << 28) /* 1 = sample on inverted pixel clock edge */
#define LTDC_GCR_DEPOL  (1U << 29) /* 1 = DE active high    */
#define LTDC_GCR_VSPOL  (1U << 30) /* 1 = VSYNC active high */
#define LTDC_GCR_HSPOL  (1U << 31) /* 1 = HSYNC active high */
#define LTDC_SRCR_IMR   (1U << 0)  /* immediate reload */
#define LTDC_L1CR_LEN   (1U << 0)  /* layer enable     */

/* --- I2C4 @ 0x58001C00 (D3 / APB4) ---------------------------------------- */
#define I2C4_BASE     0x58001C00UL
#define I2C4_CR1      REG32(I2C4_BASE + 0x00)
#define I2C4_CR2      REG32(I2C4_BASE + 0x04)
#define I2C4_TIMINGR  REG32(I2C4_BASE + 0x10)
#define I2C4_ISR      REG32(I2C4_BASE + 0x18)
#define I2C4_ICR      REG32(I2C4_BASE + 0x1C)
#define I2C4_RXDR     REG32(I2C4_BASE + 0x24)
#define I2C4_TXDR     REG32(I2C4_BASE + 0x28)

#define I2C_CR1_PE      (1U << 0)
#define I2C_CR2_RD_WRN  (1U << 10)
#define I2C_CR2_START   (1U << 13)
#define I2C_CR2_STOP    (1U << 14)
#define I2C_CR2_AUTOEND (1U << 25)
#define I2C_CR2_NBYTES_Pos 16
#define I2C_ISR_TXIS    (1U << 1)
#define I2C_ISR_RXNE    (1U << 2)
#define I2C_ISR_NACKF   (1U << 4)
#define I2C_ISR_STOPF   (1U << 5)
#define I2C_ISR_BUSY    (1U << 15)
#define I2C_ICR_NACKCF  (1U << 4)
#define I2C_ICR_STOPCF  (1U << 5)

/* --- Cortex-M7 core registers --------------------------------------------- */
#define SCB_VTOR      REG32(0xE000ED08UL) /* vector table offset        */
#define SCB_CPACR     REG32(0xE000ED88UL) /* coprocessor (FPU) access   */

#endif /* STM32H743_REG_H */
