/*
 * QSPI NOR flash diagnostic (not part of the normal app).
 *
 * Build:  cmake --build build --target qspi-report
 * Flash:  STM32_Programmer_CLI -c port=USB1 -w build/qspi-report.hex
 *         then RESET with BOOT0 low.
 *
 * Non-destructive bring-up check for the on-board Winbond W25Q128JV (16 MB
 * QSPI serial NOR, IC4). It enables the QUADSPI controller and reads the
 * flash's JEDEC ID (cmd 0x9F) plus status register 1 (cmd 0x05), printing the
 * result over USART1 (115200 8N1, PA9/PA10) once a second. NO erase or program
 * is performed -- this only proves the chip is wired and alive before any
 * destructive read/write code is added.
 *
 *   Expected JEDEC ID: EF 40 18  (Winbond / type 0x40 / 16 MB)
 *
 * Failure modes worth knowing:
 *   - all 0x00 or all 0xFF  -> a data line (IO0/IO1) or CLK/NCS is not wired
 *   - read returns rc != 0  -> controller never completed (clock/pin issue)
 */
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include "clock.h"
#include "qspi.h"

void HardFault_Handler(void)
{
    printf("\r\n!! HARDFAULT in qspi-report !!\r\n");
    for (;;) { }
}

int main(void)
{
    uart_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\r\n==== STM32H743 QSPI flash report ====\r\n");

    if (clock_init() != 0)
        printf("[warn] clock_init failed -- QSPI kernel clock may be slow\r\n");

    qspi_init();

    for (;;) {
        uint8_t id[3] = {0};
        int rc = qspi_read_jedec_id(id);
        if (rc) {
            printf("\r\n[qspi] JEDEC ID read FAILED (rc=%d) -- check wiring/clock\r\n", rc);
        } else {
            int ok = (id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x18);
            const char *verdict =
                ok                 ? "OK (W25Q128JV)" :
                (id[0] == 0xEF)    ? "Winbond, unexpected capacity" :
                (id[0] == 0x00 || id[0] == 0xFF) ? "no response (check IO0/IO1/CLK/NCS)" :
                                     "UNEXPECTED part";
            printf("\r\n[qspi] JEDEC ID = %02X %02X %02X  -> %s\r\n",
                   id[0], id[1], id[2], verdict);
        }

        uint8_t sr1 = 0;
        if (qspi_read_status(&sr1) == 0)
            printf("[qspi] status reg1 = 0x%02X (WIP=%d WEL=%d)\r\n",
                   sr1, (int)(sr1 & 1U), (int)((sr1 >> 1) & 1U));

        clock_delay_ms(1000);
    }
}
