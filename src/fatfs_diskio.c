/*
 * FatFs <-> SD glue (the diskio layer FatFs expects the application to provide).
 * Maps FatFs's sector functions onto the bare-metal SDMMC2 driver in src/sd.c.
 *
 * Single volume (pdrv 0) = the microSD card. 512-byte sectors throughout, which
 * matches both FatFs (FF_MIN_SS == FF_MAX_SS == 512) and the SD block size.
 */
#include "ff.h"
#include "diskio.h"
#include "sd.h"

static volatile DSTATUS s_stat = STA_NOINIT;
static sd_card_t        s_card;

DSTATUS disk_status(BYTE pdrv)
{
    return (pdrv == 0) ? s_stat : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    sd_clock_init();
    s_stat = (sd_init(&s_card) == 0) ? 0 : STA_NOINIT;
    return s_stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || (s_stat & STA_NOINIT)) return RES_NOTRDY;
    return sd_read_blocks((uint32_t)sector, (uint8_t *)buff, (uint32_t)count) == 0
               ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || (s_stat & STA_NOINIT)) return RES_NOTRDY;
    return sd_write_blocks((uint32_t)sector, (const uint8_t *)buff, (uint32_t)count) == 0
               ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
    case CTRL_SYNC:                            /* writes already busy-wait done */
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = (LBA_t)(s_card.capacity_bytes / 512ULL);
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:                        /* erase block, in sectors */
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

/* No RTC on this board: hand FatFs a fixed timestamp (2026-06-25 12:00:00). */
DWORD get_fattime(void)
{
    return ((DWORD)(2026 - 1980) << 25)
         | ((DWORD)6  << 21)
         | ((DWORD)25 << 16)
         | ((DWORD)12 << 11)
         | ((DWORD)0  << 5)
         | ((DWORD)0  >> 1);
}
