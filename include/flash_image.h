#ifndef FLASH_IMAGE_H
#define FLASH_IMAGE_H

/*
 * End-to-end QSPI-flash asset demo: on the first boot it generates an RGB565
 * splash image and stores it in the on-board W25Q128JV; on every boot it loads
 * that image back from the flash (via memory-mapped reads) and blits it to the
 * centre of the LCD. Exercises the whole chain: generate -> erase/program flash
 * -> memory-mapped read -> SDRAM framebuffer -> LTDC -> panel.
 *
 * Requires qspi_init(), sdram_init() and lcd_init() to have run first.
 */
void flash_image_show(void);

#endif /* FLASH_IMAGE_H */
