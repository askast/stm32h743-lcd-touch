#ifndef SDRAM_H
#define SDRAM_H

#include <stdint.h>
#include "stm32h743_reg.h"

/* External SDRAM is on FMC bank 1, mapped at 0xC0000000 (verified against the
 * board vendor's CubeIDE project). The W9825G6KH is 32 MB (13 row / 9 col /
 * 4 banks / 16-bit). */
#define SDRAM_BASE  SDRAM_BANK1_BASE
#define SDRAM_SIZE  (32U * 1024U * 1024U)

/* Configure the FMC pins and SDRAM controller, then run the JEDEC init
 * sequence. clock_init() must have run first (needs HCLK = 200 MHz).
 * Returns 0 on success, or non-zero if the controller stayed busy (so it can
 * never hang the boot). */
int sdram_init(void);

/* Write/verify a region of SDRAM. Returns the number of mismatching words
 * (0 = pass). Tests `words` 32-bit cells starting at SDRAM_BASE. */
uint32_t sdram_selftest(uint32_t words);

#endif /* SDRAM_H */
