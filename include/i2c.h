#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stddef.h>

/* Minimal bare-metal I2C4 master for the GT911 touch controller.
 * Pins: SCL=PD12, SDA=PD13 (AF4, open-drain). Kernel clock = D3PCLK1.
 * clock_init() must run first. */
void i2c4_init(void);

/* 7-bit-addressed write/read. Return 0 on success, non-zero on NACK/timeout. */
int i2c4_write(uint8_t addr7, const uint8_t *data, size_t len);
int i2c4_read(uint8_t addr7, uint8_t *buf, size_t len);

#endif /* I2C_H */
