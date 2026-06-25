#ifndef LCD_H
#define LCD_H

#include <stdint.h>

/* 7" 1024x600 RGB IPS panel driven by the LTDC, framebuffer in external SDRAM
 * (RGB565). sdram_init() must run before lcd_init(). */
#define LCD_WIDTH   1024U
#define LCD_HEIGHT  600U

/* RGB888 -> RGB565 */
#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | (((b) & 0xF8U) >> 3)))

/* Panel horizontal/vertical timing (in pixels/lines). */
typedef struct {
    uint16_t hsw, vsw;   /* sync pulse widths   */
    uint16_t hbp, vbp;   /* back porches        */
    uint16_t hfp, vfp;   /* front porches       */
} lcd_timing_t;

/* Configure LTDC pins, the controller, the single RGB565 layer, and turn the
 * panel + backlight on. */
void lcd_init(void);

/* Re-program just the LTDC timing + sync polarities (pol = OR of LTDC_GCR_*POL
 * bits) live, without touching the pins or layer. Used to hunt the panel's
 * correct mode. */
void lcd_set_mode(const lcd_timing_t *t, uint32_t pol);

/* Fill the whole framebuffer with one RGB565 color. */
void lcd_fill(uint16_t color);

/* Draw 8 vertical color bars (a quick "is the panel alive / wired right" test). */
void lcd_test_pattern(void);

/* Draw a small filled square (radius 4) centered at (cx, cy). */
void lcd_draw_dot(uint16_t cx, uint16_t cy, uint16_t color);

/* Copy a w x h RGB565 bitmap (row-major, `src[row*w + col]`) into the
 * framebuffer with its top-left at (x, y). Clipped to the panel edges. */
void lcd_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *src);

#endif /* LCD_H */
