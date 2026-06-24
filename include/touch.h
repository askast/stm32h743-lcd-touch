#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>

/* GT911 capacitive touch over I2C4 (addr 0x5D). Pins: SCL=PD12, SDA=PD13,
 * RST=PD11, INT=PH7. clock_init() must run first. Prints the controller ID. */
void touch_init(void);

/* Poll for a touch. Returns 1 and fills *x,*y (panel pixels) if a finger is
 * down, else 0. Reads only the first touch point. */
int touch_read(uint16_t *x, uint16_t *y);

#endif /* TOUCH_H */
