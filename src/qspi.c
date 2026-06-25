#include "qspi.h"
#include "stm32h743_reg.h"
#include "gpio_af.h"

/*
 * Bare-metal QUADSPI read-only driver for the W25Q128JV. Pin map and bank from
 * the board schematic (dkm1978/STM32H7IIT6-Core-board, IC4 = W25Q128JVSQ):
 *
 *   CLK = PB2 (AF9)   NCS = PB6 (AF10)
 *   IO0 = PF8 (AF10)  IO1 = PF9 (AF10)  IO2 = PF7 (AF9)  IO3 = PF6 (AF9)
 *
 * Only single-line indirect reads are used, so IO2/IO3 (/WP, /HOLD) just need
 * to idle high -- which the flash does internally once /CS is controlled. We
 * still map them so a later quad-mode path needs no pin changes.
 */

#define W25Q_CMD_JEDEC_ID      0x9FU /* -> 3 bytes: manufacturer, type, capacity */
#define W25Q_CMD_READ_SR1      0x05U /* -> 1 byte status register 1              */
#define W25Q_CMD_WRITE_ENABLE  0x06U /* set WEL latch                            */
#define W25Q_CMD_SECTOR_ERASE  0x20U /* erase 4 KB sector at 24-bit address      */
#define W25Q_CMD_PAGE_PROGRAM  0x02U /* program <=256 B at 24-bit address        */
#define W25Q_CMD_READ_DATA     0x03U /* read at 24-bit address (no dummy cycles) */

#define W25Q_SR1_WIP       0x01U    /* write/erase in progress                  */

#define QSPI_ADSIZE_24BIT  2U       /* CCR ADSIZE field: 3 address bytes         */

#define FLASH_SIZE_LOG2    24U      /* 16 MB = 2^24 bytes (FSIZE = this - 1)     */

/* Byte-wide view of the data register: each byte read pops exactly one byte
 * out of the FIFO (a 32-bit read would pop four). */
static volatile uint8_t *const QSPI_DR_BYTE =
    (volatile uint8_t *)(QUADSPI_BASE + 0x20);

/* Bounded spin until any bit in `mask` is set in SR. Returns 1 on timeout. */
static int qspi_wait(uint32_t mask)
{
    uint32_t n = 200000U;
    while (!(QUADSPI_SR & mask) && n) { n--; }
    return (n == 0);
}

/* Bounded spin until the controller is idle. Returns 1 on timeout. */
static int qspi_wait_idle(void)
{
    uint32_t n = 200000U;
    while ((QUADSPI_SR & QSPI_SR_BUSY) && n) { n--; }
    return (n == 0);
}

void qspi_init(void)
{
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOFEN;
    RCC_AHB3ENR |= RCC_AHB3ENR_QSPIEN;
    (void)RCC_AHB3ENR;

    gpio_init_af(GPIOB_BASE, 2, 9,  3U);   /* CLK */
    gpio_init_af(GPIOB_BASE, 6, 10, 3U);   /* NCS */
    gpio_init_af(GPIOF_BASE, 6, 9,  3U);   /* IO3 */
    gpio_init_af(GPIOF_BASE, 7, 9,  3U);   /* IO2 */
    gpio_init_af(GPIOF_BASE, 8, 10, 3U);   /* IO0 */
    gpio_init_af(GPIOF_BASE, 9, 10, 3U);   /* IO1 */

    /* Prescaler /8 off the QSPI kernel clock (rcc_hclk3 = 200 MHz after
     * clock_init) -> ~25 MHz, conservative for a dev-board trace. FTHRES = 0
     * so FTF asserts as soon as >= 1 byte is in the FIFO. */
    QUADSPI_CR = 0;
    QUADSPI_CR = (7U << QSPI_CR_PRESCALER_Pos);

    /* 16 MB device (FSIZE = log2(bytes) - 1 = 23); /CS high >= 2 cycles. */
    QUADSPI_DCR = ((FLASH_SIZE_LOG2 - 1U) << QSPI_DCR_FSIZE_Pos)
                | (1U << QSPI_DCR_CSHT_Pos);

    QUADSPI_CR |= QSPI_CR_EN;
}

/* Issue a single-line instruction with no address and read `len` data bytes on
 * a single line (indirect read). The transfer starts the instant CCR is written
 * because there is no address phase. Returns 0 on success. */
static int qspi_cmd_read(uint8_t instr, uint8_t *buf, uint32_t len)
{
    if (qspi_wait_idle()) return 1;

    QUADSPI_FCR = QSPI_FCR_CTCF | QSPI_FCR_CTEF;   /* clear stale flags     */
    QUADSPI_DLR = len - 1U;                        /* DLR holds bytes - 1   */
    QUADSPI_CCR = ((uint32_t)QSPI_FMODE_IND_READ << QSPI_CCR_FMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE    << QSPI_CCR_DMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE    << QSPI_CCR_IMODE_Pos)
                | instr;

    for (uint32_t i = 0; i < len; i++) {
        if (qspi_wait(QSPI_SR_FTF | QSPI_SR_TCF)) return 2;
        buf[i] = *QSPI_DR_BYTE;
    }

    if (qspi_wait(QSPI_SR_TCF)) return 3;
    QUADSPI_FCR = QSPI_FCR_CTCF;
    return 0;
}

int qspi_read_jedec_id(uint8_t id[3])
{
    return qspi_cmd_read(W25Q_CMD_JEDEC_ID, id, 3);
}

int qspi_read_status(uint8_t *sr1)
{
    return qspi_cmd_read(W25Q_CMD_READ_SR1, sr1, 1);
}

/* ---- DESTRUCTIVE path ----------------------------------------------------- */

int qspi_wait_ready(void)
{
    /* Each iteration is a full status read, so this bounds total time well
     * beyond the 4 KB sector-erase max (~400 ms) without a wall clock. */
    for (uint32_t n = 0; n < 2000000U; n++) {
        uint8_t sr;
        if (qspi_read_status(&sr)) return 1;
        if (!(sr & W25Q_SR1_WIP)) return 0;
    }
    return 2;
}

int qspi_write_enable(void)
{
    if (qspi_wait_idle()) return 1;
    QUADSPI_FCR = QSPI_FCR_CTCF | QSPI_FCR_CTEF;
    /* Instruction-only transaction: starts as soon as CCR is written. */
    QUADSPI_CCR = ((uint32_t)QSPI_FMODE_IND_WRITE << QSPI_CCR_FMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE     << QSPI_CCR_IMODE_Pos)
                | W25Q_CMD_WRITE_ENABLE;
    return qspi_wait_idle() ? 2 : 0;
}

int qspi_erase_sector(uint32_t addr)
{
    if (qspi_write_enable()) return 1;
    if (qspi_wait_idle())    return 2;

    QUADSPI_FCR = QSPI_FCR_CTCF | QSPI_FCR_CTEF;
    /* Instruction + 24-bit address, no data. With a data phase absent, the
     * transaction launches when QUADSPI_AR is written. */
    QUADSPI_CCR = ((uint32_t)QSPI_FMODE_IND_WRITE  << QSPI_CCR_FMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE      << QSPI_CCR_ADMODE_Pos)
                | ((uint32_t)QSPI_ADSIZE_24BIT     << QSPI_CCR_ADSIZE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE      << QSPI_CCR_IMODE_Pos)
                | W25Q_CMD_SECTOR_ERASE;
    QUADSPI_AR = addr;

    if (qspi_wait_idle()) return 3;
    return qspi_wait_ready();   /* WIP stays set for the whole erase */
}

int qspi_program(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (len == 0U || len > 256U) return 10;
    if (qspi_write_enable())     return 1;
    if (qspi_wait_idle())        return 2;

    QUADSPI_FCR = QSPI_FCR_CTCF | QSPI_FCR_CTEF;
    QUADSPI_DLR = len - 1U;
    QUADSPI_CCR = ((uint32_t)QSPI_FMODE_IND_WRITE  << QSPI_CCR_FMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE      << QSPI_CCR_DMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE      << QSPI_CCR_ADMODE_Pos)
                | ((uint32_t)QSPI_ADSIZE_24BIT     << QSPI_CCR_ADSIZE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE      << QSPI_CCR_IMODE_Pos)
                | W25Q_CMD_PAGE_PROGRAM;
    QUADSPI_AR = addr;

    for (uint32_t i = 0; i < len; i++) {
        if (qspi_wait(QSPI_SR_FTF)) return 3;   /* FIFO has room */
        *QSPI_DR_BYTE = data[i];
    }
    if (qspi_wait(QSPI_SR_TCF)) return 4;
    QUADSPI_FCR = QSPI_FCR_CTCF;

    return qspi_wait_ready();    /* page program ~0.7 ms typical */
}

int qspi_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (len == 0U)        return 10;
    if (qspi_wait_idle()) return 1;

    QUADSPI_FCR = QSPI_FCR_CTCF | QSPI_FCR_CTEF;
    QUADSPI_DLR = len - 1U;
    QUADSPI_CCR = ((uint32_t)QSPI_FMODE_IND_READ   << QSPI_CCR_FMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE      << QSPI_CCR_DMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE      << QSPI_CCR_ADMODE_Pos)
                | ((uint32_t)QSPI_ADSIZE_24BIT     << QSPI_CCR_ADSIZE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE      << QSPI_CCR_IMODE_Pos)
                | W25Q_CMD_READ_DATA;
    QUADSPI_AR = addr;          /* read launches on AR write */

    for (uint32_t i = 0; i < len; i++) {
        if (qspi_wait(QSPI_SR_FTF | QSPI_SR_TCF)) return 2;
        buf[i] = *QSPI_DR_BYTE;
    }
    if (qspi_wait(QSPI_SR_TCF)) return 3;
    QUADSPI_FCR = QSPI_FCR_CTCF;
    return 0;
}

int qspi_program_buffer(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t off = 0;
    while (off < len) {
        /* Never cross a 256-byte page boundary in a single program command. */
        uint32_t page_rem = 256U - ((addr + off) & 0xFFU);
        uint32_t chunk = (len - off < page_rem) ? (len - off) : page_rem;
        int rc = qspi_program(addr + off, data + off, chunk);
        if (rc) return rc;
        off += chunk;
    }
    return 0;
}

/* ---- Memory-mapped mode --------------------------------------------------- */

void qspi_memmap_enable(void)
{
    if (qspi_wait_idle()) return;
    QUADSPI_FCR = QSPI_FCR_CTCF | QSPI_FCR_CTEF;
    /* Same single-line 0x03 read we validated, but FMODE = memory-mapped: no
     * DLR/AR -- the controller fetches on each access to QSPI_MEMMAP_BASE. */
    QUADSPI_CCR = ((uint32_t)QSPI_FMODE_MEMMAP   << QSPI_CCR_FMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE    << QSPI_CCR_DMODE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE    << QSPI_CCR_ADMODE_Pos)
                | ((uint32_t)QSPI_ADSIZE_24BIT   << QSPI_CCR_ADSIZE_Pos)
                | ((uint32_t)QSPI_MODE_SINGLE    << QSPI_CCR_IMODE_Pos)
                | W25Q_CMD_READ_DATA;
}

void qspi_memmap_disable(void)
{
    /* Abort the auto-fetch state machine and wait for it to settle so indirect
     * commands work again. */
    QUADSPI_CR |= QSPI_CR_ABORT;
    uint32_t n = 100000U;
    while ((QUADSPI_CR & QSPI_CR_ABORT) && n) { n--; }
    (void)qspi_wait_idle();
    QUADSPI_FCR = QSPI_FCR_CTCF | QSPI_FCR_CTEF;
}

int qspi_memmap_read(uint32_t addr, void *buf, uint32_t len)
{
    if (((uint64_t)addr + len) > (1ULL << FLASH_SIZE_LOG2)) return 1;

    qspi_memmap_enable();
    const volatile uint8_t *src = (const volatile uint8_t *)(QSPI_MEMMAP_BASE + addr);
    uint8_t *dst = (uint8_t *)buf;
    for (uint32_t i = 0; i < len; i++) dst[i] = src[i];
    qspi_memmap_disable();
    return 0;
}
