#include "i2c.h"
#include "stm32h743_reg.h"
#include "gpio_af.h"

/*
 * Bare-metal I2C4 master. SCL=PD12, SDA=PD13 (AF4, open-drain). The board has
 * external pull-ups on the touch I2C lines, so no internal pull is used.
 *
 * The TIMINGR value is the board vendor's CubeMX value (~85 kHz at our 100 MHz
 * kernel clock) -- well within the GT911's range. I2C4 kernel = D3PCLK1
 * (rcc_pclk4), which is the reset-default mux selection.
 */
#define I2C4_TIMING  0x307075B1UL

#define WAIT(cond)  do { uint32_t _n = 100000U; \
                         while (!(cond) && _n) { _n--; } \
                         if (!_n) return 1; } while (0)

void i2c4_init(void)
{
    RCC_APB4ENR |= RCC_APB4ENR_I2C4EN;
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIODEN;
    (void)RCC_APB4ENR;

    gpio_init_af_od(GPIOD_BASE, 12, 4U, 2U);   /* SCL */
    gpio_init_af_od(GPIOD_BASE, 13, 4U, 2U);   /* SDA */

    I2C4_CR1 = 0;                              /* disable while configuring */
    I2C4_TIMINGR = I2C4_TIMING;
    I2C4_CR1 = I2C_CR1_PE;
}

int i2c4_write(uint8_t addr7, const uint8_t *data, size_t len)
{
    WAIT(!(I2C4_ISR & I2C_ISR_BUSY));
    I2C4_CR2 = ((uint32_t)addr7 << 1)
             | ((uint32_t)len << I2C_CR2_NBYTES_Pos)
             | I2C_CR2_AUTOEND | I2C_CR2_START;   /* write, auto STOP */

    for (size_t i = 0; i < len; i++) {
        WAIT(I2C4_ISR & (I2C_ISR_TXIS | I2C_ISR_NACKF));
        if (I2C4_ISR & I2C_ISR_NACKF) { I2C4_ICR = I2C_ICR_NACKCF; return 2; }
        I2C4_TXDR = data[i];
    }
    WAIT(I2C4_ISR & I2C_ISR_STOPF);
    I2C4_ICR = I2C_ICR_STOPCF;
    return 0;
}

int i2c4_read(uint8_t addr7, uint8_t *buf, size_t len)
{
    WAIT(!(I2C4_ISR & I2C_ISR_BUSY));
    I2C4_CR2 = ((uint32_t)addr7 << 1)
             | ((uint32_t)len << I2C_CR2_NBYTES_Pos)
             | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND | I2C_CR2_START;  /* read, auto STOP */

    for (size_t i = 0; i < len; i++) {
        WAIT(I2C4_ISR & (I2C_ISR_RXNE | I2C_ISR_NACKF));
        if (I2C4_ISR & I2C_ISR_NACKF) { I2C4_ICR = I2C_ICR_NACKCF; return 2; }
        buf[i] = (uint8_t)I2C4_RXDR;
    }
    WAIT(I2C4_ISR & I2C_ISR_STOPF);
    I2C4_ICR = I2C_ICR_STOPCF;
    return 0;
}
