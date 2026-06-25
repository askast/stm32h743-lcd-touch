#include "sd.h"
#include "stm32h743_reg.h"
#include "gpio_af.h"
#include "clock.h"

/*
 * SDMMC2 SD-card driver. Pin map from the board schematic:
 *   CK = PD6 (AF11)   CMD = PD7 (AF11)
 *   D0 = PB14         D1 = PB15        D2 = PB3        D3 = PB4   (all AF9)
 *
 * Init sequence (SD physical-layer spec): CMD0 -> CMD8 -> ACMD41 (HCS) -> CMD2
 * -> CMD3 -> CMD9 -> CMD7 -> ACMD6 (4-bit) -> CMD16. Data uses the DPSM + FIFO
 * polling with hardware flow control on (HWFC_EN) to avoid FIFO over-runs.
 */

/* SD command indices */
#define CMD0_GO_IDLE        0U
#define CMD2_ALL_SEND_CID   2U
#define CMD3_SEND_RCA       3U
#define CMD7_SELECT         7U
#define CMD8_SEND_IF_COND   8U
#define CMD9_SEND_CSD       9U
#define CMD12_STOP          12U
#define CMD13_SEND_STATUS   13U
#define CMD16_SET_BLOCKLEN  16U
#define CMD17_READ_SINGLE   17U
#define CMD18_READ_MULTI    18U
#define CMD24_WRITE_SINGLE  24U
#define CMD25_WRITE_MULTI   25U
#define CMD55_APP_CMD       55U
#define ACMD6_SET_BUSWIDTH  6U
#define ACMD41_SD_OP_COND   41U

/* Response kinds */
enum { RESP_NONE = 0, RESP_SHORT, RESP_SHORT_NOCRC, RESP_LONG };

static uint16_t s_rca;              /* cached during init for ACMDs        */
static uint8_t  s_block_addressing = 1; /* 1 = SDHC/SDXC (LBA), 0 = byte addr */

void sd_clock_init(void)
{
    /* PLL2: HSI 64 MHz / DIVM2=16 -> 4 MHz; N=50 -> 200 MHz VCO; R=2 -> 100 MHz.
     * (Same recipe as the LTDC's PLL3, just a different output divider.) */
    RCC_PLLCKSELR = (RCC_PLLCKSELR & ~(0x3FU << RCC_DIVM2_Pos))
                  | (16U << RCC_DIVM2_Pos);
    RCC_PLLCFGR  |= (2U << RCC_PLL2RGE_Pos) | RCC_DIVR2EN;   /* 4-8 MHz in, R out */
    RCC_PLL2DIVR  = ((50U - 1U) << 0) | ((2U - 1U) << 24);  /* N=50, R=2 */

    RCC_CR |= RCC_CR_PLL2ON;
    uint32_t n = 2000000U;
    while (!(RCC_CR & RCC_CR_PLL2RDY) && n) { n--; }

    /* SDMMC kernel clock = pll2_r_ck; enable the SDMMC2 peripheral clock. */
    RCC_D1CCIPR |= RCC_SDMMCSEL_PLL2;
    RCC_AHB2ENR |= RCC_AHB2ENR_SDMMC2EN;
    (void)RCC_AHB2ENR;
}

/* AF + very-high speed + pull-up (SD lines idle high). */
static void sd_pin(uint32_t base, uint32_t pin, uint32_t af)
{
    gpio_init_af(base, pin, af, 3U);
    GPIO_PUPDR(base) = (GPIO_PUPDR(base) & ~(3U << (pin * 2))) | (1U << (pin * 2));
}

static void sd_gpio_init(void)
{
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIODEN;
    (void)RCC_AHB4ENR;

    sd_pin(GPIOD_BASE, 6, 11);   /* CK  */
    sd_pin(GPIOD_BASE, 7, 11);   /* CMD */
    sd_pin(GPIOB_BASE, 14, 9);   /* D0  */
    sd_pin(GPIOB_BASE, 15, 9);   /* D1  */
    sd_pin(GPIOB_BASE, 3,  9);   /* D2  */
    sd_pin(GPIOB_BASE, 4,  9);   /* D3  */
}

/* Send a command and wait for the response (or timeout). `resp` is RESP_*.
 * `data` sets CMDTRANS for data-transfer commands. Returns 0 on success. */
static int sd_cmd(uint32_t index, uint32_t arg, int resp, int data)
{
    SDMMC2_ICR = SDMMC_ICR_STATIC;          /* clear stale flags */
    SDMMC2_ARG = arg;

    uint32_t cmd = index | SDMMC_CMD_CPSMEN;
    if (resp == RESP_LONG)            cmd |= SDMMC_CMD_WAITRESP_LONG;
    else if (resp != RESP_NONE)       cmd |= SDMMC_CMD_WAITRESP_SHORT;
    if (data)                         cmd |= SDMMC_CMD_CMDTRANS;
    SDMMC2_CMD = cmd;

    uint32_t n = 1000000U;
    if (resp == RESP_NONE) {
        while (!(SDMMC2_STA & (SDMMC_STA_CMDSENT | SDMMC_STA_CTIMEOUT)) && n) n--;
    } else {
        while (!(SDMMC2_STA & (SDMMC_STA_CMDREND | SDMMC_STA_CCRCFAIL |
                               SDMMC_STA_CTIMEOUT)) && n) n--;
    }
    if (n == 0) return 1;
    if (SDMMC2_STA & SDMMC_STA_CTIMEOUT) { SDMMC2_ICR = SDMMC_ICR_STATIC; return 2; }
    /* R3 (OCR) and similar carry no valid CRC -> CCRCFAIL is expected there. */
    if ((SDMMC2_STA & SDMMC_STA_CCRCFAIL) && resp != RESP_SHORT_NOCRC) {
        SDMMC2_ICR = SDMMC_ICR_STATIC;
        return 3;
    }
    SDMMC2_ICR = SDMMC_ICR_STATIC;
    return 0;
}

/* Application command: CMD55 (with RCA) then the ACMD. */
static int sd_acmd(uint32_t index, uint32_t arg, int resp)
{
    int rc = sd_cmd(CMD55_APP_CMD, (uint32_t)s_rca << 16, RESP_SHORT, 0);
    if (rc) return rc;
    return sd_cmd(index, arg, resp, 0);
}

int sd_init(sd_card_t *card)
{
    s_rca = 0;
    for (int i = 0; i < 16; i++) { card->cid[i] = 0; card->csd[i] = 0; }
    card->high_capacity = 0; card->v2 = 0; card->rca = 0; card->capacity_bytes = 0;

    sd_gpio_init();

    /* Power up the SDMMC at 400 kHz (kernel 100 MHz / (2*125)). Flow control on. */
    SDMMC2_POWER = 0;
    SDMMC2_CLKCR = 125U | SDMMC_CLKCR_HWFC_EN;
    SDMMC2_POWER = SDMMC_POWER_ON;
    clock_delay_ms(2);                      /* >= 74 card clocks of ramp-up */

    /* CMD0: go idle (no response). */
    if (sd_cmd(CMD0_GO_IDLE, 0, RESP_NONE, 0)) return 10;

    /* CMD8: voltage check (0x1AA). If it echoes back, the card is v2.0+. */
    if (sd_cmd(CMD8_SEND_IF_COND, 0x1AA, RESP_SHORT, 0) == 0 &&
        (SDMMC2_RESP1 & 0xFFF) == 0x1AA) {
        card->v2 = 1;
    }

    /* ACMD41: negotiate operating conditions. HCS set if v2 (host supports SDHC).
     * Loop until the card clears its busy bit (OCR bit31). */
    uint32_t ocr_arg = 0x00FF8000U | (card->v2 ? 0x40000000U : 0U);
    uint32_t ocr = 0, tries = 0;
    do {
        if (sd_acmd(ACMD41_SD_OP_COND, ocr_arg, RESP_SHORT_NOCRC)) return 11;
        ocr = SDMMC2_RESP1;
        if (++tries > 1000U) return 12;     /* ~ up to a second of polling */
        clock_delay_ms(1);
    } while (!(ocr & 0x80000000U));
    card->high_capacity = (ocr & 0x40000000U) ? 1 : 0;
    s_block_addressing = card->high_capacity;

    /* CMD2: get CID (long response). */
    if (sd_cmd(CMD2_ALL_SEND_CID, 0, RESP_LONG, 0)) return 13;
    uint32_t r[4] = { SDMMC2_RESP1, SDMMC2_RESP2, SDMMC2_RESP3, SDMMC2_RESP4 };
    for (int i = 0; i < 4; i++) {
        card->cid[i*4+0] = (uint8_t)(r[i] >> 24); card->cid[i*4+1] = (uint8_t)(r[i] >> 16);
        card->cid[i*4+2] = (uint8_t)(r[i] >> 8);  card->cid[i*4+3] = (uint8_t)(r[i]);
    }

    /* CMD3: ask the card to publish its RCA. */
    if (sd_cmd(CMD3_SEND_RCA, 0, RESP_SHORT, 0)) return 14;
    s_rca = (uint16_t)(SDMMC2_RESP1 >> 16);
    card->rca = s_rca;

    /* CMD9: read CSD (long response) and decode capacity. */
    if (sd_cmd(CMD9_SEND_CSD, (uint32_t)s_rca << 16, RESP_LONG, 0)) return 15;
    uint32_t c[4] = { SDMMC2_RESP1, SDMMC2_RESP2, SDMMC2_RESP3, SDMMC2_RESP4 };
    for (int i = 0; i < 4; i++) {
        card->csd[i*4+0] = (uint8_t)(c[i] >> 24); card->csd[i*4+1] = (uint8_t)(c[i] >> 16);
        card->csd[i*4+2] = (uint8_t)(c[i] >> 8);  card->csd[i*4+3] = (uint8_t)(c[i]);
    }
    uint32_t csd_ver = (c[0] >> 30) & 0x3U;
    if (csd_ver == 1U) {                    /* CSD v2 (SDHC/SDXC) */
        uint32_t c_size = ((c[1] & 0x3FU) << 16) | ((c[2] >> 16) & 0xFFFFU);
        card->capacity_bytes = (uint64_t)(c_size + 1U) * 512ULL * 1024ULL;
    } else {                                /* CSD v1 (SDSC) */
        uint32_t c_size      = ((c[1] & 0x3FFU) << 2) | ((c[2] >> 30) & 0x3U);
        uint32_t c_size_mult = (c[2] >> 15) & 0x7U;
        uint32_t read_bl_len = (c[1] >> 16) & 0xFU;
        uint32_t blocknr     = (c_size + 1U) << (c_size_mult + 2U);
        card->capacity_bytes = (uint64_t)blocknr * (1ULL << read_bl_len);
    }

    /* CMD7: select the card (move to transfer state). */
    if (sd_cmd(CMD7_SELECT, (uint32_t)s_rca << 16, RESP_SHORT, 0)) return 16;

    /* ACMD6: switch to the 4-bit bus, then mirror that in CLKCR. */
    if (sd_acmd(ACMD6_SET_BUSWIDTH, 2U, RESP_SHORT)) return 17;

    /* Step up to ~25 MHz (100 MHz / (2*2)) and 4-bit. */
    SDMMC2_CLKCR = 2U | SDMMC_CLKCR_HWFC_EN | SDMMC_CLKCR_WIDBUS_4;

    /* CMD16: fix block length at 512 (harmless on SDHC, required on SDSC). */
    if (sd_cmd(CMD16_SET_BLOCKLEN, 512U, RESP_SHORT, 0)) return 18;

    return 0;
}

/* SDHC/SDXC address by block; SDSC by byte (decided during sd_init). */
static uint32_t sd_addr(uint32_t lba)
{
    return s_block_addressing ? lba : (lba * 512U);
}

/* Poll CMD13 until the card leaves the programming state and is ready for data.
 * Used after writes (the card holds D0 low while it programs). */
static int sd_wait_ready(void)
{
    for (uint32_t i = 0; i < 100000U; i++) {
        if (sd_cmd(CMD13_SEND_STATUS, (uint32_t)s_rca << 16, RESP_SHORT, 0)) return 26;
        uint32_t st = SDMMC2_RESP1;
        uint32_t state = (st >> 9) & 0xFU;        /* CURRENT_STATE */
        if ((st & (1U << 8)) && state != 7U) return 0;   /* READY_FOR_DATA, not prg */
        clock_delay_ms(1);
    }
    return 27;
}

/* Stop an open-ended multi-block transfer. */
static int sd_stop(void)
{
    return sd_cmd(CMD12_STOP, 0, RESP_SHORT, 0) ? 28 : 0;
}

/* Shared block data transfer. One of rbuf/wbuf is used per `write`. The data
 * path is set up BEFORE the command: on the H7 SDMMC, CMDTRANS auto-enables the
 * DPSM when the response arrives, so DCTRL.DTEN must NOT be set here. */
static int sd_data_xfer(uint32_t cmd, uint32_t arg,
                        uint8_t *rbuf, const uint8_t *wbuf,
                        uint32_t bytes, int write)
{
    SDMMC2_DTIMER = 0x00FFFFFFU;
    SDMMC2_DLEN   = bytes;
    SDMMC2_DCTRL  = SDMMC_DCTRL_DBLOCK_512 | (write ? 0U : SDMMC_DCTRL_DTDIR_RX);

    int crc = sd_cmd(cmd, arg, RESP_SHORT, 1);
    if (crc) return 30 + crc;        /* 31=timeout 32=CTIMEOUT 33=CCRCFAIL */

    const uint32_t words = bytes / 4U;
    uint32_t i = 0, guard = 50000000U;

    if (write) {
        const uint32_t *p = (const uint32_t *)(const void *)wbuf;
        while (i < words && guard--) {
            uint32_t sta = SDMMC2_STA;
            if (sta & (SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT | SDMMC_STA_TXUNDERR)) {
                SDMMC2_ICR = SDMMC_ICR_STATIC;
                return 24;
            }
            if (!(sta & SDMMC_STA_TXFIFOF)) SDMMC2_FIFO = p[i++];
        }
    } else {
        uint32_t *p = (uint32_t *)(void *)rbuf;
        while (i < words && guard--) {
            uint32_t sta = SDMMC2_STA;
            if (sta & (SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT | SDMMC_STA_RXOVERR)) {
                SDMMC2_ICR = SDMMC_ICR_STATIC;
                return 21;
            }
            if (!(sta & SDMMC_STA_RXFIFOE)) p[i++] = SDMMC2_FIFO;
        }
    }

    uint32_t n = 2000000U;
    while (!(SDMMC2_STA & (SDMMC_STA_DATAEND | SDMMC_STA_DCRCFAIL |
                           SDMMC_STA_DTIMEOUT)) && n) n--;
    int err = (SDMMC2_STA & (SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT)) ? 22 : 0;
    SDMMC2_ICR = SDMMC_ICR_STATIC;
    if (i != words && !err) err = 25;
    return err;
}

int sd_read_block(uint32_t lba, uint8_t *buf)
{
    return sd_data_xfer(CMD17_READ_SINGLE, sd_addr(lba), buf, 0, 512U, 0);
}

int sd_write_block(uint32_t lba, const uint8_t *buf)
{
    int rc = sd_data_xfer(CMD24_WRITE_SINGLE, sd_addr(lba), 0, buf, 512U, 1);
    return rc ? rc : sd_wait_ready();
}

int sd_read_blocks(uint32_t lba, uint8_t *buf, uint32_t count)
{
    if (count == 0U) return 0;
    if (count == 1U) return sd_read_block(lba, buf);
    int rc  = sd_data_xfer(CMD18_READ_MULTI, sd_addr(lba), buf, 0, count * 512U, 0);
    int src = sd_stop();
    return rc ? rc : src;
}

int sd_write_blocks(uint32_t lba, const uint8_t *buf, uint32_t count)
{
    if (count == 0U) return 0;
    if (count == 1U) return sd_write_block(lba, buf);
    int rc  = sd_data_xfer(CMD25_WRITE_MULTI, sd_addr(lba), 0, buf, count * 512U, 1);
    int src = sd_stop();
    int wrc = sd_wait_ready();
    return rc ? rc : (src ? src : wrc);
}
