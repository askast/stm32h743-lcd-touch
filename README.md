# stm32h743-lcd-touch

A self-contained, bare-metal starting template for an **STM32H743IIT6** core
board (`dkm1978/STM32H7IIT6-Core-board`). It boots the chip to 400 MHz, brings
up the 32 MB external SDRAM, drives a 7" **1024×600 RGB565 LCD** over LTDC, and
reads the **GT911 capacitive touch** controller over I²C.

Out of the box `main()` shows an 8-bar color splash, then lets you drag a finger
to draw on a black canvas. Use it as a clean base for your own H7 LCD/touch
firmware — every peripheral is a small, standalone module you can keep or strip.

**No STM32CubeMX, HAL, CMSIS, or vendor SDK** — the project ships its own minimal
register header, startup/vector table, and linker script, so it builds from just
the Arm GCC toolchain.

## Target hardware

- **MCU:** STM32H743IIT6 (Cortex-M7, 2 MB flash, 1 MB RAM, LQFP176)
- **Clock:** 400 MHz SYSCLK / 200 MHz HCLK, PLLs driven from the **64 MHz HSI**
- **SDRAM:** 32 MB Winbond W9825G6KH on FMC bank 1 @ `0xC0000000`
- **LCD:** 7" 1024×600 RGB565 panel on LTDC (backlight enable on PH6)
- **Touch:** GT911 on hardware I²C4 (SCL=PD12, SDA=PD13), addr 0x5D
- **Serial:** USART1, **PA9 = TX**, **PA10 = RX**, **115200 8N1**

## Prerequisites

- **Arm GNU toolchain** (`arm-none-eabi-gcc`, `objcopy`, `size`) on `PATH`
- **CMake** ≥ 3.20 and a generator (**Ninja** recommended, or Make)
- **STM32CubeProgrammer** (`STM32_Programmer_CLI`) for flashing

## Build

```sh
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
cmake --build build
```

Outputs land in `build/`:
- `stm32h743-lcd-touch.elf` — debugging
- `stm32h743-lcd-touch.hex` / `.bin` — flashing
- `stm32h743-lcd-touch.map` — symbol/section map

For a release build, add `-DCMAKE_BUILD_TYPE=Release` to the configure step.

## Flash

This board has **no ST-Link/SWD wired** — use the built-in ROM bootloader. Hold
**BOOT0 high + RESET** to enter it, flash, then **press RESET with BOOT0 low** to
run the app.

```sh
# USB DFU (board's native USB in DFU mode):
STM32_Programmer_CLI -c port=USB1 -w build/stm32h743-lcd-touch.hex

# UART system bootloader (8E1!) on PA9/PA10 via a USB-TTL adapter:
STM32_Programmer_CLI -c port=COMx br=115200 -w build/stm32h743-lcd-touch.hex
```

The app's own serial output is **8N1** (the UART bootloader uses 8E1). On a
terminal at 115200 you should see the bring-up log:

```
==== STM32H743 LCD bring-up ====
[boot] ready -- drag to draw.
```

## Project layout

| Path | Purpose |
| --- | --- |
| `src/main.c` | Entry point: clock → SDRAM → LCD splash → GT911 touch-draw loop |
| `src/clock.c` | PLL config: 400 MHz SYSCLK + PLL3-R 25 MHz LTDC pixel clock |
| `src/sdram.c` | FMC bring-up for the 32 MB external SDRAM @ `0xC0000000` |
| `src/lcd.c` | LTDC + 1024×600 RGB565 panel; fill / dot / color-bar helpers |
| `src/touch.c` | GT911 capacitive touch driver |
| `src/i2c.c` | Register-level I²C4 master (no HAL) |
| `src/uart.c` | USART1 bring-up and blocking `uart_write()` |
| `src/syscalls.c` | newlib stubs; `_write()` retargets `printf` to USART1 |
| `src/startup_stm32h743.c` | Vector table, `Reset_Handler`, `SystemInit` (FPU/VTOR/clock) |
| `include/stm32h743_reg.h` | Minimal register definitions (RCC/PWR/FMC/LTDC/GPIO/USART) |
| `linker/STM32H743IITX_FLASH.ld` | Memory map: code in FLASH, data/stack in DTCM |
| `cmake/arm-none-eabi-gcc.cmake` | CMake toolchain file for arm-none-eabi |

See [`CLAUDE.md`](CLAUDE.md) for the hard-won board specifics (exact pin maps,
panel timing, and H7 gotchas).

## Next steps

- Bring up the on-board **NOR flash** (not yet initialized).
- Add a real framebuffer/graphics layer on top of `lcd.c`.
- Move peripheral interrupts in by extending the vector table in
  `src/startup_stm32h743.c`.
