/*
 * microSD (SDMMC2) diagnostic -- read-only (not part of the normal app).
 *
 * Build:  cmake --build build --target sd-report
 * Flash:  STM32_Programmer_CLI -c port=USB1 -w build/sd-report.hex
 *         then RESET with BOOT0 low.
 *
 * Brings up SDMMC2, initialises the inserted card, and reports its type,
 * capacity, RCA and CID over USART1 (115200 8N1). Then reads block 0 (the MBR)
 * and checks for the 0x55AA boot signature, dumping the first 16 bytes. It only
 * READS -- nothing on the card is modified. Loops once a second so a serial
 * listener catches it whenever it attaches.
 */
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include "clock.h"
#include "sd.h"

void HardFault_Handler(void)
{
    printf("\r\n!! HARDFAULT in sd-report !!\r\n");
    for (;;) { }
}

int main(void)
{
    uart_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\r\n==== STM32H743 microSD (SDMMC2) report ====\r\n");

    if (clock_init() != 0)
        printf("[warn] clock_init failed\r\n");
    sd_clock_init();

    sd_card_t card;
    int rc = sd_init(&card);

    uint8_t blk[512];
    int brc = -1;
    if (rc == 0) brc = sd_read_block(0, blk);

    for (;;) {
        printf("\r\n---- SD status ----\r\n");
        if (rc != 0) {
            printf("sd_init FAILED at step %d (no card? wiring? clock?)\r\n", rc);
        } else {
            printf("card: %s, %s\r\n",
                   card.v2 ? "v2.0+" : "v1.x",
                   card.high_capacity ? "SDHC/SDXC (block-addressed)" : "SDSC (byte-addressed)");
            printf("RCA : 0x%04X\r\n", card.rca);
            printf("size: %lu MB\r\n",
                   (unsigned long)(card.capacity_bytes / (1024ULL * 1024ULL)));
            printf("CID :");
            for (int i = 0; i < 16; i++) printf(" %02X", card.cid[i]);
            printf("\r\n");

            if (brc == 0) {
                int mbr = (blk[510] == 0x55 && blk[511] == 0xAA);
                printf("block0 read OK -- MBR sig %s (%02X %02X)\r\n",
                       mbr ? "present" : "absent", blk[510], blk[511]);
                printf("block0[0..15]:");
                for (int i = 0; i < 16; i++) printf(" %02X", blk[i]);
                printf("\r\n");
            } else {
                printf("block0 read FAILED (rc=%d)\r\n", brc);
            }
        }
        clock_delay_ms(1000);
    }
}
