#include "lcd.h"
#include "sdram.h"
#include "clock.h"
#include "gpio_af.h"

/*
 * LTDC bring-up for this board's 7" 1024x600 RGB panel on its 40-pin FPC (CN1).
 *
 * The panel is wired RGB565: only R3-R7, G2-G7, B3-B7 are connected, plus
 * CLK/DE/HS/VS. Pin map taken from the board schematic (dkm1978/
 * STM32H7IIT6-Core-board) -- it differs from the Waveshare reference, and in
 * particular does NOT use PH2/PH3 (those are this board's SDRAM SDCKE0/SDNE0).
 *
 * Panel control pins on the FPC: PH6, PH7, PE2, PE3 (display/backlight enable
 * and touch reset/irq). We drive all four high to enable display + backlight.
 *
 * Framebuffer: single RGB565 layer at the base of external SDRAM (0xC0000000).
 */

static volatile uint16_t *const g_fb = (volatile uint16_t *)SDRAM_BASE;

/* {port base, pin, alternate function} for every connected LTDC signal.
 * Most are AF14; the two "secondary" LTDC mappings (G3 on PG10, B4 on PG12)
 * are AF9. */
static const struct { uint32_t base; uint8_t pin; uint8_t af; } k_ltdc[] = {
    {GPIOH_BASE, 9,  14}, /* R3  */
    {GPIOH_BASE, 10, 14}, /* R4  */
    {GPIOH_BASE, 11, 14}, /* R5  */
    {GPIOH_BASE, 12, 14}, /* R6  */
    {GPIOG_BASE, 6,  14}, /* R7  */
    {GPIOH_BASE, 13, 14}, /* G2  */
    {GPIOG_BASE, 10, 9 }, /* G3  (AF9) */
    {GPIOH_BASE, 15, 14}, /* G4  */
    {GPIOI_BASE, 0,  14}, /* G5  */
    {GPIOI_BASE, 1,  14}, /* G6  */
    {GPIOI_BASE, 2,  14}, /* G7  */
    {GPIOA_BASE, 8,  13}, /* B3  (AF13) */
    {GPIOG_BASE, 12, 9 }, /* B4  (AF9) */
    {GPIOI_BASE, 5,  14}, /* B5  */
    {GPIOI_BASE, 6,  14}, /* B6  */
    {GPIOI_BASE, 7,  14}, /* B7  */
    {GPIOF_BASE, 10, 14}, /* DE  */
    {GPIOI_BASE, 9,  14}, /* VSYNC */
    {GPIOI_BASE, 10, 14}, /* HSYNC */
};

/* Backlight enable = PH6 (LCD_BL), driven high. (PH7 is the touch INT and is
 * owned by the touch driver; PE2/PE3 are unused on this board.) */
static const struct { uint32_t base; uint8_t pin; } k_ctrl[] = {
    {GPIOH_BASE, 6},
};

static void lcd_gpio_init(void)
{
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN
                 | RCC_AHB4ENR_GPIOGEN | RCC_AHB4ENR_GPIOHEN | RCC_AHB4ENR_GPIOIEN;
    RCC_APB3ENR |= RCC_APB3ENR_LTDCEN;
    (void)RCC_APB3ENR;

    for (unsigned i = 0; i < sizeof(k_ltdc) / sizeof(k_ltdc[0]); i++) {
        gpio_init_af(k_ltdc[i].base, k_ltdc[i].pin, k_ltdc[i].af, 3U);
    }
    /* Pixel clock benefits from very-high speed. */
    gpio_init_af(GPIOG_BASE, 7, 14, 3U);

    /* Drive the panel control pins high (display + backlight enable). */
    for (unsigned i = 0; i < sizeof(k_ctrl) / sizeof(k_ctrl[0]); i++) {
        gpio_init_output(k_ctrl[i].base, k_ctrl[i].pin);
        GPIO_BSRR(k_ctrl[i].base) = (1U << k_ctrl[i].pin);
    }
}

void lcd_set_mode(const lcd_timing_t *t, uint32_t pol)
{
    uint16_t accHBP = t->hsw + t->hbp - 1U;
    uint16_t accVBP = t->vsw + t->vbp - 1U;
    uint16_t accAW  = accHBP + LCD_WIDTH;
    uint16_t accAH  = accVBP + LCD_HEIGHT;
    uint16_t totW   = accAW + t->hfp;
    uint16_t totH   = accAH + t->vfp;

    LTDC_GCR &= ~LTDC_GCR_LTDCEN;          /* disable while reprogramming */
    LTDC_SSCR = ((t->hsw - 1U) << 16) | (t->vsw - 1U);
    LTDC_BPCR = (accHBP << 16) | accVBP;
    LTDC_AWCR = (accAW  << 16) | accAH;
    LTDC_TWCR = (totW   << 16) | totH;
    LTDC_L1WHPCR = (accAW << 16) | (accHBP + 1U);
    LTDC_L1WVPCR = (accAH << 16) | (accVBP + 1U);
    LTDC_SRCR = LTDC_SRCR_IMR;             /* reload */
    /* Set the four polarity bits (top nibble) and re-enable. */
    LTDC_GCR = (LTDC_GCR & 0x0FFFFFFFU) | (pol & 0xF0000000U) | LTDC_GCR_LTDCEN;
}

void lcd_init(void)
{
    lcd_gpio_init();

    LTDC_BCCR = 0;                          /* black background */

    /* Layer 1: full-screen RGB565 framebuffer in SDRAM. Window is set by
     * lcd_set_mode(); the rest is mode-independent. */
    LTDC_L1PFCR   = 2U;                     /* RGB565 */
    LTDC_L1CACR   = 0xFFU;                  /* constant alpha = opaque */
    LTDC_L1DCCR   = 0;
    LTDC_L1BFCR   = (4U << 8) | 5U;         /* BF1=const alpha, BF2=1-const alpha */
    LTDC_L1CFBAR  = SDRAM_BASE;
    LTDC_L1CFBLR  = (2048U << 16) | (2048U + 3U);  /* pitch=2048, line len+3 */
    LTDC_L1CFBLNR = LCD_HEIGHT;
    LTDC_L1CR     = LTDC_L1CR_LEN;          /* enable layer */

    /* This panel's exact timing (from the board vendor's LTDC example), all
     * sync active-low, non-inverted pixel clock. */
    static const lcd_timing_t panel = {1, 3, 46, 23, 40, 10};
    lcd_set_mode(&panel, 0);
}

void lcd_fill(uint16_t color)
{
    for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        g_fb[i] = color;
    }
}

void lcd_draw_dot(uint16_t cx, uint16_t cy, uint16_t color)
{
    const int r = 4;
    for (int dy = -r; dy <= r; dy++) {
        int py = (int)cy + dy;
        if (py < 0 || py >= (int)LCD_HEIGHT) continue;
        for (int dx = -r; dx <= r; dx++) {
            int px = (int)cx + dx;
            if (px < 0 || px >= (int)LCD_WIDTH) continue;
            g_fb[py * LCD_WIDTH + px] = color;
        }
    }
}

void lcd_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *src)
{
    for (uint16_t row = 0; row < h; row++) {
        uint32_t py = (uint32_t)y + row;
        if (py >= LCD_HEIGHT) break;
        for (uint16_t col = 0; col < w; col++) {
            uint32_t px = (uint32_t)x + col;
            if (px >= LCD_WIDTH) continue;
            g_fb[py * LCD_WIDTH + px] = src[(uint32_t)row * w + col];
        }
    }
}

void lcd_test_pattern(void)
{
    static const uint16_t bars[8] = {
        RGB565(255, 255, 255),  /* white   */
        RGB565(255, 255, 0),    /* yellow  */
        RGB565(0,   255, 255),  /* cyan    */
        RGB565(0,   255, 0),    /* green   */
        RGB565(255, 0,   255),  /* magenta */
        RGB565(255, 0,   0),    /* red     */
        RGB565(0,   0,   255),  /* blue    */
        RGB565(0,   0,   0),    /* black   */
    };
    const uint32_t bar_w = LCD_WIDTH / 8U;

    for (uint32_t y = 0; y < LCD_HEIGHT; y++) {
        volatile uint16_t *row = &g_fb[y * LCD_WIDTH];
        for (uint32_t x = 0; x < LCD_WIDTH; x++) {
            uint32_t b = x / bar_w;
            row[x] = bars[b > 7 ? 7 : b];
        }
    }
}
