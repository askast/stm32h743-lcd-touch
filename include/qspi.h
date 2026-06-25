#ifndef QSPI_H
#define QSPI_H

#include <stdint.h>

/*
 * Minimal QUADSPI master for the on-board Winbond W25Q128JV (16 MB serial NOR)
 * on bank 1. Single-line indirect mode at ~25 MHz.
 *
 * This is intentionally READ-ONLY / non-destructive: it configures the
 * controller + pins and issues read-only commands (JEDEC ID, status). No
 * erase or program path exists yet -- add one once the ID read confirms the
 * chip is wired correctly. clock_init() should run first so the QSPI kernel
 * clock (rcc_hclk3) is at a known 200 MHz.
 */

/* Configure the 6 QSPI pins and the controller, then enable it. */
void qspi_init(void);

/* Read the 3-byte JEDEC ID (cmd 0x9F). Returns 0 on success.
 * A healthy W25Q128JV returns { 0xEF, 0x40, 0x18 }. */
int qspi_read_jedec_id(uint8_t id[3]);

/* Read status register 1 (cmd 0x05). Returns 0 on success; *sr1 = value
 * (bit0 = WIP/busy, bit1 = WEL/write-enable-latch). */
int qspi_read_status(uint8_t *sr1);

/* ---- DESTRUCTIVE primitives (modify flash contents) ---------------------- *
 * All take a byte address in the 0..16MB device space (NOT the 0x90000000
 * memory-mapped alias) and return 0 on success, non-zero on error/timeout. */

/* Poll status until the write-in-progress (WIP) bit clears. */
int qspi_wait_ready(void);

/* Set the write-enable latch (cmd 0x06). Required before each erase/program. */
int qspi_write_enable(void);

/* Erase the 4 KB sector containing `addr` (cmd 0x20). Blocks until done. */
int qspi_erase_sector(uint32_t addr);

/* Program up to 256 bytes within a single page (cmd 0x02). `len` must be
 * 1..256 and must not cross a 256-byte page boundary. Blocks until done. */
int qspi_program(uint32_t addr, const uint8_t *data, uint32_t len);

/* Read `len` bytes starting at `addr` (cmd 0x03). */
int qspi_read(uint32_t addr, uint8_t *buf, uint32_t len);

/* Program an arbitrary-length buffer, splitting it across 256-byte page
 * boundaries as needed (cmd 0x02 per page). The target range must already be
 * erased. Returns 0 on success. */
int qspi_program_buffer(uint32_t addr, const uint8_t *data, uint32_t len);

/* ---- Memory-mapped mode -------------------------------------------------- *
 * Put the controller in memory-mapped mode so the whole 16 MB device appears
 * read-only at QSPI_MEMMAP_BASE and can be read with a plain pointer (the
 * QUADSPI issues the 0x03 read for you on each CPU access). Handy for pulling
 * assets straight into the LTDC framebuffer.
 *
 * While memory-mapped mode is active the indirect commands above (read/erase/
 * program) must NOT be used -- call qspi_memmap_disable() first, which aborts
 * back to indirect mode. */
#define QSPI_MEMMAP_BASE  0x90000000UL

void qspi_memmap_enable(void);
void qspi_memmap_disable(void);

/* One-shot convenience: enable memory-mapped mode, copy `len` bytes from device
 * offset `addr` into `buf`, then return to indirect mode. Returns 0 on success. */
int qspi_memmap_read(uint32_t addr, void *buf, uint32_t len);

#endif /* QSPI_H */
