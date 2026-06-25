#ifndef SD_H
#define SD_H

#include <stdint.h>

/*
 * Minimal bare-metal SD-card driver on hardware SDMMC2 (no HAL). Supports the
 * common case: SDHC/SDXC (block-addressed) and SDSC cards, 4-bit bus, default
 * speed (~25 MHz), single-block reads via FIFO polling. Read-only for now.
 *
 * The SDMMC kernel clock is driven from PLL2-R (100 MHz) by sd_clock_init() so
 * the project's main clock tree (clock.c / PLL1) is left untouched.
 */

typedef struct {
    uint8_t  high_capacity;   /* 1 = SDHC/SDXC (LBA addressing)        */
    uint8_t  v2;              /* 1 = v2.0+ card (responded to CMD8)    */
    uint16_t rca;             /* relative card address (upper 16 bits) */
    uint64_t capacity_bytes;  /* from CSD                              */
    uint8_t  cid[16];         /* raw CID                               */
    uint8_t  csd[16];         /* raw CSD                               */
} sd_card_t;

/* Configure PLL2-R and the SDMMC2 kernel/peripheral clocks. Call once before
 * sd_init(). clock_init() should have run first. */
void sd_clock_init(void);

/* Initialise the card. Returns 0 on success, or a non-zero step code marking
 * which phase failed (handy for bring-up over UART). */
int sd_init(sd_card_t *card);

/* Read one 512-byte block at logical block address `lba` into `buf` (>=512 B).
 * Returns 0 on success. */
int sd_read_block(uint32_t lba, uint8_t *buf);

/* Write one 512-byte block from `buf` to `lba`, then wait for the card to
 * finish programming. Returns 0 on success. DESTRUCTIVE. */
int sd_write_block(uint32_t lba, const uint8_t *buf);

/* Read `count` consecutive 512-byte blocks starting at `lba` into `buf`
 * (>= count*512 bytes). Uses CMD18 + CMD12. Returns 0 on success. */
int sd_read_blocks(uint32_t lba, uint8_t *buf, uint32_t count);

/* Write `count` consecutive 512-byte blocks from `buf` starting at `lba`.
 * Uses CMD25 + CMD12, then waits for programming to finish. DESTRUCTIVE.
 * Returns 0 on success. */
int sd_write_blocks(uint32_t lba, const uint8_t *buf, uint32_t count);

#endif /* SD_H */
