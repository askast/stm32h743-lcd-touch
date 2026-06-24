#ifndef GPIO_AF_H
#define GPIO_AF_H

#include <stdint.h>
#include "stm32h743_reg.h"

/* Configure one pin to an alternate function (push-pull, no pull). speed: 0..3. */
static inline void gpio_init_af(uint32_t base, uint32_t pin, uint32_t af, uint32_t speed)
{
    GPIO_MODER(base)   = (GPIO_MODER(base)   & ~(3U << (pin * 2))) | (2U << (pin * 2));
    GPIO_OTYPER(base)  &= ~(1U << pin);
    GPIO_OSPEEDR(base) = (GPIO_OSPEEDR(base) & ~(3U << (pin * 2))) | (speed << (pin * 2));
    GPIO_PUPDR(base)   &= ~(3U << (pin * 2));
    if (pin < 8) {
        GPIO_AFRL(base) = (GPIO_AFRL(base) & ~(0xFU << (pin * 4))) | (af << (pin * 4));
    } else {
        GPIO_AFRH(base) = (GPIO_AFRH(base) & ~(0xFU << ((pin - 8) * 4))) | (af << ((pin - 8) * 4));
    }
}

/* Configure one pin to an alternate function in OPEN-DRAIN mode (for I2C). */
static inline void gpio_init_af_od(uint32_t base, uint32_t pin, uint32_t af, uint32_t speed)
{
    gpio_init_af(base, pin, af, speed);
    GPIO_OTYPER(base) |= (1U << pin);   /* open-drain */
}

/* Configure one pin as a push-pull output (low speed, no pull). */
static inline void gpio_init_output(uint32_t base, uint32_t pin)
{
    GPIO_MODER(base) = (GPIO_MODER(base) & ~(3U << (pin * 2))) | (1U << (pin * 2));
    GPIO_OTYPER(base)  &= ~(1U << pin);
    GPIO_PUPDR(base)   &= ~(3U << (pin * 2));
}

#endif /* GPIO_AF_H */
