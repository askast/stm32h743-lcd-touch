#include "sdram.h"
#include "clock.h"
#include "gpio_af.h"

/*
 * FMC SDRAM driver for the on-board 32 MB W9825G6KH on FMC BANK 1 (0xC0000000).
 *
 * Pin map, bank, geometry and timings are from the board vendor's own CubeIDE
 * SDRAM_TEST project (dkm1978/STM32H7IIT6-Core-board). Note this board uses
 * bank 1 (SDNE0=PH3, SDCKE0=PH2) -- NOT bank 2 like the Waveshare reference.
 *
 * All FMC signals use AF12 at very-high speed.
 */

#define AF_FMC   12U
#define SPD_VH   3U   /* very high */

#define CTB1     (1U << 4)   /* SDCMR command target bank 1 */

/* SDCMR command modes */
#define CMD_CLK_ENABLE   1U
#define CMD_PALL         2U
#define CMD_AUTOREFRESH  3U
#define CMD_LOAD_MODE    4U

/* Mode register: CAS=3, burst length 1, sequential, single-location write. */
#define SDRAM_MODEREG  (0x0030U | 0x0000U | 0x0000U | 0x0200U)

/* Refresh: 8192 rows, 64 ms, 100 MHz SDRAM clock:
 * (64e-3 * 100e6 / 8192) - 20 ~= 761. */
#define SDRAM_REFRESH  761U

static void sdram_gpio_init(void)
{
    /* Clock the GPIO ports used by the FMC and the FMC itself. */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIODEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN
                 | RCC_AHB4ENR_GPIOGEN | RCC_AHB4ENR_GPIOHEN;
    RCC_AHB3ENR |= RCC_AHB3ENR_FMCEN;
    (void)RCC_AHB4ENR;

    /* Port D: D0=PD14 D1=PD15 D2=PD0 D3=PD1 D13=PD8 D14=PD9 D15=PD10 */
    static const uint8_t pd[] = {0, 1, 8, 9, 10, 14, 15};
    for (unsigned i = 0; i < sizeof(pd); i++) gpio_init_af(GPIOD_BASE, pd[i], AF_FMC, SPD_VH);

    /* Port E: NBL0=PE0 NBL1=PE1 D4..D12=PE7..PE15 */
    static const uint8_t pe[] = {0, 1, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    for (unsigned i = 0; i < sizeof(pe); i++) gpio_init_af(GPIOE_BASE, pe[i], AF_FMC, SPD_VH);

    /* Port F: A0..A5=PF0..PF5, SDNRAS=PF11, A6..A9=PF12..PF15 */
    static const uint8_t pf[] = {0, 1, 2, 3, 4, 5, 11, 12, 13, 14, 15};
    for (unsigned i = 0; i < sizeof(pf); i++) gpio_init_af(GPIOF_BASE, pf[i], AF_FMC, SPD_VH);

    /* Port G: A10=PG0 A11=PG1 A12=PG2 BA0=PG4 BA1=PG5 SDCLK=PG8 SDNCAS=PG15 */
    static const uint8_t pg[] = {0, 1, 2, 4, 5, 8, 15};
    for (unsigned i = 0; i < sizeof(pg); i++) gpio_init_af(GPIOG_BASE, pg[i], AF_FMC, SPD_VH);

    /* Port H (BANK 1): SDCKE0=PH2 SDNE0=PH3 SDNWE=PH5 */
    static const uint8_t ph[] = {2, 3, 5};
    for (unsigned i = 0; i < sizeof(ph); i++) gpio_init_af(GPIOH_BASE, ph[i], AF_FMC, SPD_VH);
}

/* Bounded wait for the controller to go idle. Returns 1 on timeout. */
static int sdram_wait_idle(void)
{
    uint32_t n = 1000000U;
    while ((FMC_SDSR & FMC_SDSR_BUSY) && n) { n--; }
    return (n == 0);
}

static int sdram_send_cmd(uint32_t mode, uint32_t autorefresh, uint32_t mrd)
{
    if (sdram_wait_idle()) return 1;
    FMC_SDCMR = mode | CTB1 | ((autorefresh - 1U) << 5) | (mrd << 9);
    return 0;
}

int sdram_init(void)
{
    sdram_gpio_init();

    /* Bank-1 control + geometry all live in SDCR1 / SDTR1. */
    FMC_SDCR1 = (1U << 0)    /* NC   = 9 columns */
              | (2U << 2)    /* NR   = 13 rows   */
              | (1U << 4)    /* MWID = 16-bit    */
              | (1U << 6)    /* NB   = 4 banks   */
              | (3U << 7)    /* CAS  = 3 cycles  */
              | (2U << 10)   /* SDCLK = HCLK/2   */
              | (1U << 13);  /* RPIPE = 1 cycle  */

    /* Timings (cycles, fields are value-1), from the vendor project. */
    FMC_SDTR1 = ((2U - 1U) << 0)    /* TMRD */
              | ((7U - 1U) << 4)    /* TXSR */
              | ((4U - 1U) << 8)    /* TRAS */
              | ((7U - 1U) << 12)   /* TRC  */
              | ((3U - 1U) << 16)   /* TWR  */
              | ((2U - 1U) << 20)   /* TRP  */
              | ((2U - 1U) << 24);  /* TRCD */

    /* Master enable for the whole FMC controller. On the STM32H7 (unlike F4/F7)
     * this bit gates ALL FMC accesses; without it every read/write to the SDRAM
     * region hard-faults. */
    FMC_BCR1 |= FMC_BCR1_FMCEN;

    /* JEDEC power-up sequence (all targeting bank 1). */
    int to = 0;
    to |= sdram_send_cmd(CMD_CLK_ENABLE, 1, 0);
    clock_delay_ms(1);                       /* >= 100 us */
    to |= sdram_send_cmd(CMD_PALL, 1, 0);
    to |= sdram_send_cmd(CMD_AUTOREFRESH, 8, 0);
    to |= sdram_send_cmd(CMD_LOAD_MODE, 1, SDRAM_MODEREG);

    to |= sdram_wait_idle();
    FMC_SDRTR = (SDRAM_REFRESH << 1);
    return to ? 1 : 0;
}

uint32_t sdram_selftest(uint32_t words)
{
    volatile uint32_t *p = (volatile uint32_t *)SDRAM_BASE;
    uint32_t errors = 0;

    for (uint32_t i = 0; i < words; i++) {
        p[i] = i * 2654435761U;   /* unique per-address pattern (Knuth hash) */
    }
    for (uint32_t i = 0; i < words; i++) {
        if (p[i] != i * 2654435761U) {
            errors++;
        }
    }
    return errors;
}
