/*
 * QSPI NOR flash SELF-TEST (DESTRUCTIVE -- not part of the normal app).
 *
 * Build:  cmake --build build --target qspi-selftest
 * Flash:  STM32_Programmer_CLI -c port=COM5 br=115200 -w build/qspi-selftest.hex
 *         then RESET with BOOT0 low.
 *
 * This erases and reprograms ONE 4 KB sector of the on-board W25Q128JV to prove
 * the full read/erase/program path -- not just the JEDEC ID. It only touches
 * the chosen sector; it does NOT touch the MCU's internal flash (your firmware)
 * and does NOT make anything unrecoverable. NOR flash is rewritable ~100k times.
 *
 * The scratch sector is the LAST one in the device (0x00FFF000) so it never
 * collides with anything you might later store at the base of the flash.
 *
 * Output on USART1 (115200 8N1, PA9/PA10):
 *   [1] JEDEC ID check
 *   [2] erase sector
 *   [3] confirm erased (all 0xFF)
 *   [4] page program a known pattern
 *   [5] read back + compare  -> PASS / FAIL
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "uart.h"
#include "clock.h"
#include "qspi.h"

#define TEST_ADDR  0x00FFF000UL    /* last 4 KB sector of the 16 MB device */
#define N          32U             /* bytes to program/verify              */

void HardFault_Handler(void)
{
    printf("\r\n!! HARDFAULT in qspi-selftest !!\r\n");
    for (;;) { }
}

int main(void)
{
    uart_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\r\n==== STM32H743 QSPI flash SELF-TEST (destructive) ====\r\n");
    printf("Erases + reprograms one 4KB sector at 0x%08lX (last sector).\r\n",
           (unsigned long)TEST_ADDR);

    if (clock_init() != 0)
        printf("[warn] clock_init failed -- QSPI kernel clock may be slow\r\n");
    qspi_init();

    /* [1] identity */
    uint8_t id[3] = {0};
    int idok = (qspi_read_jedec_id(id) == 0) &&
               id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x18;
    printf("[1] JEDEC ID = %02X %02X %02X -> %s\r\n",
           id[0], id[1], id[2], idok ? "OK" : "BAD");

    /* known, non-trivial pattern */
    uint8_t wr[N], rd[N];
    for (uint32_t i = 0; i < N; i++) wr[i] = (uint8_t)(0xA5u ^ (i * 7u));

    /* [2] erase */
    int erase_rc = qspi_erase_sector(TEST_ADDR);
    printf("[2] erase sector            rc=%d\r\n", erase_rc);

    /* [3] confirm erased -> all 0xFF */
    int r3 = qspi_read(TEST_ADDR, rd, N);
    int erased_ok = (r3 == 0);
    for (uint32_t i = 0; i < N; i++) if (rd[i] != 0xFF) erased_ok = 0;
    printf("[3] post-erase read         rc=%d -> %s (expect all 0xFF)\r\n",
           r3, erased_ok ? "all 0xFF" : "NOT erased");

    /* [4] program */
    int prog_rc = qspi_program(TEST_ADDR, wr, N);
    printf("[4] page program            rc=%d\r\n", prog_rc);

    /* [5] read back + compare */
    int r5 = qspi_read(TEST_ADDR, rd, N);
    int verify_ok = (r5 == 0) && (memcmp(wr, rd, N) == 0);
    printf("[5] verify read             rc=%d -> %s\r\n",
           r5, verify_ok ? "MATCH" : "MISMATCH");

    int pass = idok && erase_rc == 0 && erased_ok && prog_rc == 0 && verify_ok;
    printf("\r\nRESULT: %s\r\n",
           pass ? "PASS -- full read/erase/program path verified"
                : "FAIL -- see step codes above");

    for (;;) {
        printf("\r\n[selftest] %s  (id=%02X%02X%02X erase=%d erased=%d prog=%d verify=%d)\r\n",
               pass ? "PASS" : "FAIL", id[0], id[1], id[2],
               erase_rc, erased_ok, prog_rc, verify_ok);
        printf("   wrote:");
        for (uint32_t i = 0; i < N; i++) printf(" %02X", wr[i]);
        printf("\r\n   read :");
        for (uint32_t i = 0; i < N; i++) printf(" %02X", rd[i]);
        printf("\r\n");
        clock_delay_ms(1500);
    }
}
