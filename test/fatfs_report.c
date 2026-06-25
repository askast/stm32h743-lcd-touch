/*
 * FatFs-on-microSD diagnostic -- read-only (not part of the normal app).
 *
 * Build:  cmake --build build --target fatfs-report
 * Flash:  STM32_Programmer_CLI -c port=USB1 -w build/fatfs-report.hex
 *         then RESET with BOOT0 low.
 *
 * Mounts the FAT filesystem on the inserted microSD (FatFs R0.15 over the
 * SDMMC2 driver), then lists the root directory and reports total/free space
 * over USART1 (115200 8N1). If a file named "HELLO.TXT" exists it prints its
 * first line. It only READS -- nothing on the card is modified. Loops once a
 * second so a serial listener catches it whenever it attaches.
 */
#include <stdio.h>
#include <string.h>
#include "uart.h"
#include "clock.h"
#include "ff.h"

void HardFault_Handler(void)
{
    printf("\r\n!! HARDFAULT in fatfs-report !!\r\n");
    for (;;) { }
}

static FATFS s_fs;

static void list_root(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK) { printf("  f_opendir failed (%d)\r\n", fr); return; }

    int n = 0;
    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        printf("  %c %10lu  %s\r\n",
               (fno.fattrib & AM_DIR) ? 'D' : '-',
               (unsigned long)fno.fsize, fno.fname);
        n++;
    }
    f_closedir(&dir);
    printf("  (%d entries)\r\n", n);
}

static void show_free(void)
{
    DWORD fre_clust;
    FATFS *fsp;
    if (f_getfree("", &fre_clust, &fsp) != FR_OK) return;
    DWORD tot_sect = (fsp->n_fatent - 2) * fsp->csize;   /* 512-byte sectors */
    DWORD fre_sect = fre_clust * fsp->csize;
    printf("FS: %lu MB total, %lu MB free (cluster = %u sectors)\r\n",
           (unsigned long)(tot_sect / 2048UL),
           (unsigned long)(fre_sect / 2048UL),
           (unsigned)fsp->csize);
}

static void show_hello(void)
{
    FIL fp;
    if (f_open(&fp, "HELLO.TXT", FA_READ) != FR_OK) return;
    char line[64] = {0};
    if (f_gets(line, sizeof line, &fp)) {
        size_t n = strlen(line);
        while (n && (line[n-1] == '\r' || line[n-1] == '\n')) line[--n] = 0;
        printf("HELLO.TXT: \"%s\"\r\n", line);
    }
    f_close(&fp);
}

int main(void)
{
    uart_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\r\n==== STM32H743 FatFs (microSD) report ====\r\n");

    if (clock_init() != 0) printf("[warn] clock_init failed\r\n");

    FRESULT fr = f_mount(&s_fs, "", 1);   /* 1 = mount now (calls disk_initialize) */

    for (;;) {
        printf("\r\n---- FatFs status ----\r\n");
        if (fr != FR_OK) {
            printf("f_mount FAILED (%d) -- no card / not FAT / wiring?\r\n", fr);
        } else {
            printf("mounted, FAT type %u. Root listing:\r\n", s_fs.fs_type);
            list_root();
            show_free();
            show_hello();
        }
        clock_delay_ms(2000);
    }
}
