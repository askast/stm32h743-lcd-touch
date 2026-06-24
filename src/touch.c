#include "touch.h"
#include "i2c.h"
#include "clock.h"
#include "gpio_af.h"
#include <stdio.h>

/*
 * GT911 capacitive touch driver (register map + protocol from the board
 * vendor's example). Hardware I2C4 at 7-bit address 0x5D.
 *
 *   RST = PD11   INT = PH7
 *
 * Reset/address-select sequence: hold RST low with INT low, release RST (INT
 * low selects I2C address 0x5D), then release INT as an input.
 */

#define GT911_ADDR   0x5DU
#define GT_PID_REG   0x8140U   /* product ID (4 ASCII bytes) */
#define GT_STAT_REG  0x814EU   /* status / touch count       */
#define GT_TP1_REG   0x8150U   /* first touch point          */

#define RST_PORT  GPIOD_BASE
#define RST_PIN   11U
#define INT_PORT  GPIOH_BASE
#define INT_PIN   7U

static int gt_read(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t a[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    if (i2c4_write(GT911_ADDR, a, 2)) return 1;
    return i2c4_read(GT911_ADDR, buf, len);
}

static int gt_write1(uint16_t reg, uint8_t val)
{
    uint8_t a[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val };
    return i2c4_write(GT911_ADDR, a, 3);
}

void touch_init(void)
{
    i2c4_init();

    /* RST (PD11) and INT (PH7) as push-pull outputs. */
    gpio_init_output(RST_PORT, RST_PIN);
    gpio_init_output(INT_PORT, INT_PIN);

    /* Reset pulse with INT held low -> I2C address 0x5D. */
    GPIO_BSRR(RST_PORT) = (1U << (RST_PIN + 16));   /* RST low  */
    GPIO_BSRR(INT_PORT) = (1U << (INT_PIN + 16));   /* INT low  */
    clock_delay_ms(20);
    GPIO_BSRR(RST_PORT) = (1U << RST_PIN);          /* RST high */
    clock_delay_ms(10);
    GPIO_MODER(INT_PORT) &= ~(3U << (INT_PIN * 2)); /* INT -> input (float) */
    clock_delay_ms(100);                            /* let the GT911 boot   */

    uint8_t id[5] = {0};
    if (gt_read(GT_PID_REG, id, 4) == 0) {
        printf("[touch] GT911 ID: %s\r\n", id);
    } else {
        printf("[touch] GT911 not responding on I2C4\r\n");
    }
}

int touch_read(uint16_t *x, uint16_t *y)
{
    uint8_t status = 0;
    if (gt_read(GT_STAT_REG, &status, 1)) return 0;
    if (!(status & 0x80U)) return 0;     /* no new data ready */

    int touched = 0;
    if ((status & 0x0FU) > 0) {           /* at least one point */
        uint8_t b[4] = {0};
        if (gt_read(GT_TP1_REG, b, 4) == 0) {
            *x = (uint16_t)((b[1] << 8) | b[0]);
            *y = (uint16_t)((b[3] << 8) | b[2]);
            touched = 1;
        }
    }
    gt_write1(GT_STAT_REG, 0);            /* clear the status flag */
    return touched;
}
