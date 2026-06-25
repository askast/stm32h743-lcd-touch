#include "flash_image.h"
#include "qspi.h"
#include "lcd.h"
#include "sdram.h"
#include <stdio.h>
#include <stdint.h>

/*
 * Splash-image store/show demo. See flash_image.h for the chain it exercises.
 *
 * Flash layout (device offsets, NOT the 0x90000000 alias):
 *   0x000000  8-byte header: magic(4 LE) + width(2 LE) + height(2 LE)
 *   0x001000  RGB565 pixels, row-major (kept in its own sector from the header)
 *
 * The header's magic makes the store idempotent: it is written once, then every
 * later boot finds it and skips straight to loading + displaying.
 */

#define IMG_W      200U
#define IMG_H      150U
#define IMG_MAGIC  0x494D4731UL          /* 'IMG1' little-endian */

#define HDR_ADDR   0x000000UL
#define HDR_SIZE   8U
#define IMG_ADDR   0x001000UL            /* pixels start at sector 1 */
#define IMG_BYTES  (IMG_W * IMG_H * 2U)

/* SDRAM scratch buffer, well clear of the 1024x600 framebuffer at SDRAM_BASE
 * (which is ~1.2 MB). 4 MB in is plenty of separation. */
static uint16_t *const g_scratch = (uint16_t *)(SDRAM_BASE + 0x00400000UL);

/* A recognisably "image"-like tile: RGB gradient with a white border. */
static uint16_t img_pixel(uint32_t x, uint32_t y)
{
    if (x < 3U || y < 3U || x >= IMG_W - 3U || y >= IMG_H - 3U)
        return RGB565(255, 255, 255);
    uint8_t r = (uint8_t)(x * 255U / (IMG_W - 1U));
    uint8_t g = (uint8_t)(y * 255U / (IMG_H - 1U));
    return RGB565(r, g, 128);
}

static void generate_image(uint16_t *dst)
{
    for (uint32_t y = 0; y < IMG_H; y++)
        for (uint32_t x = 0; x < IMG_W; x++)
            dst[y * IMG_W + x] = img_pixel(x, y);
}

/* Erase every 4 KB sector spanning [addr, addr+len), then program the buffer. */
static int flash_store(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t end = addr + len;
    for (uint32_t a = addr & ~0xFFFUL; a < end; a += 0x1000UL)
        if (qspi_erase_sector(a)) return 1;
    return qspi_program_buffer(addr, data, len);
}

static int image_present(void)
{
    uint8_t hdr[HDR_SIZE];
    if (qspi_read(HDR_ADDR, hdr, HDR_SIZE)) return 0;

    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8)
                   | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    uint16_t w = (uint16_t)(hdr[4] | (hdr[5] << 8));
    uint16_t h = (uint16_t)(hdr[6] | (hdr[7] << 8));
    return (magic == IMG_MAGIC && w == IMG_W && h == IMG_H);
}

void flash_image_show(void)
{
    if (!image_present()) {
        printf("[flash] no stored image -- generating + writing to flash...\r\n");
        generate_image(g_scratch);

        const uint8_t hdr[HDR_SIZE] = {
            (uint8_t)IMG_MAGIC, (uint8_t)(IMG_MAGIC >> 8),
            (uint8_t)(IMG_MAGIC >> 16), (uint8_t)(IMG_MAGIC >> 24),
            (uint8_t)IMG_W, (uint8_t)(IMG_W >> 8),
            (uint8_t)IMG_H, (uint8_t)(IMG_H >> 8),
        };
        int rc = flash_store(IMG_ADDR, (const uint8_t *)g_scratch, IMG_BYTES);
        rc |= flash_store(HDR_ADDR, hdr, HDR_SIZE);     /* header last = commit */
        printf("[flash] stored %u-byte image, rc=%d\r\n", (unsigned)IMG_BYTES, rc);
    } else {
        printf("[flash] stored image found -- loading from flash\r\n");
    }

    /* Pull the pixels out of flash with the memory-mapped wrapper, then blit. */
    if (qspi_memmap_read(IMG_ADDR, g_scratch, IMG_BYTES) == 0) {
        uint16_t x = (LCD_WIDTH  - IMG_W) / 2U;
        uint16_t y = (LCD_HEIGHT - IMG_H) / 2U;
        lcd_blit(x, y, IMG_W, IMG_H, g_scratch);
        printf("[flash] image displayed from 0x%08lX at (%u,%u)\r\n",
               (unsigned long)(QSPI_MEMMAP_BASE + IMG_ADDR), x, y);
    } else {
        printf("[flash] mem-mapped image read FAILED\r\n");
    }
}
