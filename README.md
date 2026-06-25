# stm32h743-lcd-touch

A self-contained, **bare-metal** starting template for an **STM32H743IIT6** core
board (`dkm1978/STM32H7IIT6-Core-board`) driving a 7" capacitive-touch LCD.

It takes the chip from reset all the way to an interactive display: it raises the
core to 400 MHz, brings up the 32 MB external SDRAM as a framebuffer, configures
the LTDC to drive a **1024×600 RGB565** panel, and reads finger positions from a
**GT911** capacitive-touch controller over I²C. Out of the box `main()` shows an
8-bar color splash, then clears to black and lets you **drag a finger to draw**.

The point of the repo is to be a *clean, honest base* for your own H7 LCD/touch
firmware. Every peripheral is a small, standalone module with a tiny header —
keep the ones you need, delete the rest. There is **no STM32CubeMX, HAL, CMSIS,
or vendor SDK**: the project ships its own minimal register header, C
startup/vector table, and linker script, so it builds from nothing but the Arm
GCC toolchain.

## What's brought up

Each subsystem is one source file plus a header exposing a handful of functions:

| Subsystem | What it does | Public API |
| --- | --- | --- |
| **Clock** (`clock.c`) | HSI → PLL to 400 MHz SYSCLK / 200 MHz HCLK / 100 MHz SDRAM, plus PLL3-R for the LTDC pixel clock. Keeps USART1 on HSI so the baud rate survives the switch. | `clock_init()`, `clock_delay_ms()` |
| **SDRAM** (`sdram.c`) | FMC bring-up + JEDEC init for the 32 MB chip at `0xC0000000`; optional read/write self-test. | `sdram_init()`, `sdram_selftest()` |
| **LCD** (`lcd.c`) | LTDC pins, controller, and a single RGB565 layer with the framebuffer in SDRAM; fill / dot / color-bar helpers and live timing re-tuning. | `lcd_init()`, `lcd_fill()`, `lcd_draw_dot()`, `lcd_test_pattern()`, `lcd_set_mode()` |
| **Touch** (`touch.c`) | GT911 controller over a register-level I²C4 master; returns the first touch point in panel pixels. | `touch_init()`, `touch_read()` |
| **UART** (`uart.c` + `syscalls.c`) | USART1 bring-up; `printf` is retargeted to the serial port so you get a bring-up log and `printf`-style debugging. | `uart_init()`, `uart_write()` |

Boot sequence in `main()`: **UART → clock → SDRAM → LCD splash → touch-draw
loop**. UART comes up first (on the 64 MHz HSI default) so every later step is
observable on a serial terminal, and faults are caught by a `HardFault_Handler`
that prints instead of silently parking.

## Target hardware

- **MCU:** STM32H743IIT6 (Cortex-M7, 2 MB flash, 1 MB RAM, LQFP176)
- **Clock:** 400 MHz SYSCLK / 200 MHz HCLK, PLLs driven from the **64 MHz HSI**
  (no external crystal required)
- **SDRAM:** 32 MB Winbond W9825G6KH on FMC bank 1 @ `0xC0000000`
- **LCD:** 7" 1024×600 RGB565 IPS panel on LTDC (backlight enable on PH6)
- **Touch:** GT911 on hardware I²C4 (SCL=PD12, SDA=PD13, RST=PD11, INT=PH7), addr 0x5D
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
[touch] GT911 ID: 911
[flash] W25Q JEDEC ID: EF 40 18
[flash] stored image found -- loading from flash
[flash] image displayed from 0x90001000 at (412,225)
[boot] ready -- drag to draw.
```

## Project layout

| Path | Purpose |
| --- | --- |
| `src/main.c` | Entry point: clock → SDRAM → LCD splash → flash-image → GT911 touch-draw loop |
| `src/clock.c` | PLL config: 400 MHz SYSCLK + PLL3-R LTDC pixel clock |
| `src/sdram.c` | FMC bring-up for the 32 MB external SDRAM @ `0xC0000000` |
| `src/lcd.c` | LTDC + 1024×600 RGB565 panel; fill / dot / blit / color-bar helpers |
| `src/touch.c` | GT911 capacitive touch driver |
| `src/i2c.c` | Register-level I²C4 master (no HAL) |
| `src/qspi.c` | QUADSPI driver for the 16 MB W25Q128JV NOR flash (read/erase/program + mem-mapped) |
| `src/sd.c` | SDMMC2 microSD driver: init + single/multi-block read & write |
| `src/flash_image.c` | Demo: store an RGB565 image in QSPI flash, show it from `0x90000000` |
| `src/uart.c` | USART1 bring-up and blocking `uart_write()` |
| `src/syscalls.c` | newlib stubs; `_write()` retargets `printf` to USART1 |
| `src/startup_stm32h743.c` | Vector table, `Reset_Handler`, `SystemInit` (FPU/VTOR/clock) |
| `include/stm32h743_reg.h` | Minimal register definitions (RCC/PWR/FMC/LTDC/GPIO/USART/I2C/QUADSPI/SDMMC2) |
| `linker/STM32H743IITX_FLASH.ld` | Memory map: code in FLASH, data/stack in DTCM |
| `cmake/arm-none-eabi-gcc.cmake` | CMake toolchain file for arm-none-eabi |
| `docs/BOARD_REFERENCE.md` | Full per-peripheral hardware reference (pin maps, timings, clocks) |
| `docs/PORTING.md` | How to lift these drivers into another STM32H743 project |

See [`CLAUDE.md`](CLAUDE.md) for the hard-won board specifics, and
[`docs/BOARD_REFERENCE.md`](docs/BOARD_REFERENCE.md) for the full hardware
reference (exact pin maps, panel timing, clocks, and H7 gotchas).

## Using it as a template

- **Different panel?** Adjust `LCD_WIDTH`/`LCD_HEIGHT` and the timing in `lcd.c`;
  `lcd_set_mode()` lets you re-tune sync/porches live while hunting the right mode.
- **Don't need touch or the LCD?** Drop the file from `add_executable(...)` in
  `CMakeLists.txt` and delete the module — nothing else depends on it.
- **Different UART pins/peripheral?** Edit the GPIO AF setup in `src/uart.c`
  (and the clock source if you move off HSI).
- **Adding interrupts?** Extend the vector table in `src/startup_stm32h743.c` —
  it intentionally defines only the 16 core exceptions.

## Next steps

All on-board peripherals are up (clock, SDRAM, LCD, touch, QSPI NOR flash, and
microSD read/write). Possible directions from here:

- Add a real framebuffer/graphics layer (text, shapes) on top of `lcd.c`.
- Put a filesystem (e.g. FatFs) on top of the `sd.c` block driver.
- Move peripheral handling onto interrupts (e.g. the GT911 INT line on PH7) —
  extend the vector table in `src/startup_stm32h743.c` first.
- SD high-speed modes (SDR50/104) or quad-mode (4-bit) QSPI reads for throughput.
