/*
 * microSD (SDMMC2) WRITE self-test -- read-and-restore (not part of the app).
 *
 * Build:  cmake --build build --target sd-selftest
 * Flash:  STM32_Programmer_CLI -c port=USB1 -w build/sd-selftest.hex
 *         then RESET with BOOT0 low.
 *
 * Validates the write + multi-block paths without permanently changing the
 * card: for each test it READS the original blocks, writes a pattern, reads it
 * back and compares, then WRITES THE ORIGINAL BYTES BACK. Net effect on the
 * card is zero (barring power loss mid-test). Reports PASS/FAIL over USART1.
 *
 * Tests:
 *   A. single block  (CMD24 write / CMD17 read)
 *   B. 4-block burst  (CMD25 write / CMD18 read)
 *
 * The scratch region is chosen well inside the card and restored afterwards,
 * but this DOES write to the card -- only run it if you accept that.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "uart.h"
#include "clock.h"
#include "sd.h"

#define NB   4U                 /* blocks in the multi-block test */
#define BS   512U

static uint8_t orig[NB * BS];
static uint8_t patt[NB * BS];
static uint8_t back[NB * BS];

void HardFault_Handler(void)
{
    printf("\r\n!! HARDFAULT in sd-selftest !!\r\n");
    for (;;) { }
}

int main(void)
{
    uart_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\r\n==== STM32H743 microSD WRITE self-test (read-and-restore) ====\r\n");

    if (clock_init() != 0) printf("[warn] clock_init failed\r\n");
    sd_clock_init();

    sd_card_t card;
    int irc = sd_init(&card);

    int a_pass = 0, b_pass = 0;
    int a_save = -1, a_w = -1, a_r = -1, a_match = -1, a_restore = -1;
    int b_save = -1, b_w = -1, b_r = -1, b_match = -1, b_restore = -1;
    uint32_t lba = 0x00100000UL;     /* ~512 MB in; clamped to the card below */

    if (irc == 0) {
        uint32_t last = (uint32_t)(card.capacity_bytes / BS);
        if (lba + 16U >= last) lba = (last > 32U) ? (last - 32U) : 0U;
        uint32_t mlba = lba + 8U;    /* separate region for the multi-block test */

        for (uint32_t i = 0; i < NB * BS; i++) patt[i] = (uint8_t)(0x5Au ^ (i * 31u + 7u));

        /* ---- Test A: single block ---- */
        a_save    = sd_read_block(lba, orig);
        a_w       = sd_write_block(lba, patt);
        a_r       = sd_read_block(lba, back);
        a_match   = (memcmp(patt, back, BS) == 0) ? 0 : 1;
        a_restore = sd_write_block(lba, orig);
        a_pass = !a_save && !a_w && !a_r && !a_match && !a_restore;

        /* ---- Test B: 4-block burst ---- */
        b_save    = sd_read_blocks(mlba, orig, NB);
        b_w       = sd_write_blocks(mlba, patt, NB);
        b_r       = sd_read_blocks(mlba, back, NB);
        b_match   = (memcmp(patt, back, NB * BS) == 0) ? 0 : 1;
        b_restore = sd_write_blocks(mlba, orig, NB);
        b_pass = !b_save && !b_w && !b_r && !b_match && !b_restore;
    }

    for (;;) {
        printf("\r\n---- SD write self-test ----\r\n");
        if (irc != 0) {
            printf("sd_init FAILED at step %d\r\n", irc);
        } else {
            printf("card %lu MB, scratch LBA 0x%08lX\r\n",
                   (unsigned long)(card.capacity_bytes / (1024ULL * 1024ULL)),
                   (unsigned long)lba);
            printf("A single-block : %s  (save=%d write=%d read=%d cmp=%d restore=%d)\r\n",
                   a_pass ? "PASS" : "FAIL", a_save, a_w, a_r, a_match, a_restore);
            printf("B multi-block  : %s  (save=%d write=%d read=%d cmp=%d restore=%d)\r\n",
                   b_pass ? "PASS" : "FAIL", b_save, b_w, b_r, b_match, b_restore);
            printf("RESULT: %s\r\n", (a_pass && b_pass) ? "PASS" : "FAIL");
        }
        clock_delay_ms(1500);
    }
}
