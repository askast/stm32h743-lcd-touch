# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Bare-metal firmware for an **STM32H743IIT6** core board
(`dkm1978/STM32H7IIT6-Core-board`). It is deliberately **self-contained**: no
STM32CubeMX, HAL, CMSIS, or vendor SDK. The project provides its own minimal
register header, C startup/vector table, and linker script. Keep new code in
this style unless deliberately migrating to the vendor SDK (see "Growing the
project" below).

It boots to 400 MHz, brings up the 32 MB external SDRAM, and drives the 7"
**1024×600 RGB565 LCD** (LTDC) — `main()` shows 8 vertical color bars. Serial
bring-up log is on USART1. Per-peripheral details are below; the hard-won board
specifics (exact FMC/LTDC pin maps, SDRAM bank, panel timing) also live in the
agent memory files for this project.

## Build / flash commands

There is no compiler-independent wrapper — use CMake directly. Requires
`arm-none-eabi-gcc`, CMake ≥ 3.20, and Ninja (or Make) on `PATH`.

```sh
# Configure (once, or after editing CMakeLists.txt). The toolchain file is mandatory.
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake

# Build -> build/stm32h743-lcd-touch.{elf,hex,bin,map}; prints flash/RAM usage.
cmake --build build

# Release optimization: add -DCMAKE_BUILD_TYPE=Release at configure time.
```

**Flashing this board has no ST-Link/SWD wired** — use the built-in ROM
bootloader. Enter it with **BOOT0 high + RESET**, then flash over **USB DFU** or
the **UART**:

```sh
# USB DFU (board's native USB in DFU mode):
STM32_Programmer_CLI -c port=USB1 -w build/stm32h743-lcd-touch.hex

# UART system bootloader (8E1!) on PA9/PA10 via a USB-TTL adapter:
STM32_Programmer_CLI -c port=COMx br=115200 -w build/stm32h743-lcd-touch.hex
```

After flashing, **press RESET with BOOT0 low** to run the app (the bootloader
"go"/DFU-detach does not reliably start it). The `-rst` reset command is
SWD-only and will error over UART/DFU — ignore it.

There are no tests and no linter. App **serial output is USART1 @ 115200 8N1**
on **PA9 (TX) / PA10 (RX)** — note the app uses **8N1**, while the UART
bootloader uses 8E1.

## Architecture & non-obvious constraints

The boot/runtime chain (understanding it requires reading several files together):

- **`src/startup_stm32h743.c`** owns the vector table (`g_vectors`, placed in
  `.isr_vector`) and `Reset_Handler`. The table defines **only the 16 core
  exceptions** — peripheral IRQ vectors are intentionally omitted. If you enable
  any peripheral interrupt you must extend this table, or the handler will fall
  off the end. `Reset_Handler` copies `.data`, zeros `.bss`, runs `SystemInit`,
  then calls `main`.
- **`SystemInit`** (also in startup) must run before `main` and does three
  load-bearing things: enables the **FPU** (`CPACR`) — required because the build
  uses `-mfloat-abi=hard` and any float op before this faults; sets **`VTOR`** to
  `0x08000000`; and forces **HSI = 64 MHz** (`HSIDIV = /1`). This is only the
  *initial* clock — `main` then calls `clock_init()`.
- **Clocking** (`src/clock.c`): `clock_init()` raises the chip to **400 MHz
  SYSCLK / 200 MHz HCLK** (→ 100 MHz SDRAM clock) and configures **PLL3-R = 25 MHz**
  for the LTDC pixel clock. **PLLs are driven from HSI, not the HSE crystal**
  (the crystal value is unconfirmed and may be unpopulated; HSI is
  crystal-independent and proven). Gotcha: `PWR_CR3.SCUEN` must be cleared before
  `VOSRDY` asserts for voltage scale 1. **USART1 is deliberately kept on its HSI
  kernel clock**, so `UART_CLK_HZ = 64 MHz` in `src/uart.c` stays valid across the
  switch — don't repoint USART1 to a bus clock without updating that constant.
- **`printf` → UART** is wired through `_write()` in `src/syscalls.c`, which calls
  `uart_write()`. `main` sets `setvbuf(stdout, NULL, _IONBF, 0)` so output is
  unbuffered. `_sbrk()` (heap from the linker's `_end`) backs malloc/printf.
- **SDRAM** (`src/sdram.c`): 32 MB Winbond **W9825G6KH on FMC bank 1 → mapped at
  `0xC0000000`** (13 row / 9 col, CAS 3). The framebuffer lives here; it is
  accessed via a plain pointer, **not** mapped by the linker. H7 gotcha: the
  global **`FMC_BCR1.FMCEN`** must be set or every FMC access hard-faults.
- **LCD** (`src/lcd.c`): LTDC drives the 1024×600 RGB565 panel via its 40-pin FPC.
  The exact pin map, AFs, panel timing, and the load-bearing **25 MHz pixel
  clock** come from the board vendor's own LTDC example (see project memory). The
  backlight enable is **PH6** (driven high). `lcd_set_mode()` lets you re-tune
  timing/sync-polarity live.
- **Touch** (`src/touch.c` + `src/i2c.c`): GT911 capacitive controller on
  **hardware I2C4** (SCL=PD12, SDA=PD13, AF4 open-drain), addr **0x5D**, with
  RST=PD11 / INT=PH7. `i2c.c` is a small register-level I2C master (no HAL).
  `touch_read()` returns the first touch point in panel pixels.
- **QSPI NOR flash** (`src/qspi.c`): on-board Winbond **W25Q128JV** (16 MB) on
  **QUADSPI bank 1** (CLK=PB2 AF9, NCS=PB6 AF10, IO0=PF8/IO1=PF9 AF10,
  IO2=PF7/IO3=PF6 AF9). Single-line indirect driver at ~25 MHz: JEDEC ID,
  status, write-enable/erase/program/read, plus **memory-mapped mode** (the
  device appears read-only at **`0x90000000`**, `qspi_memmap_read()` for
  one-shot copies). `src/flash_image.c` is an end-to-end demo that stores an
  RGB565 splash in the flash and blits it to the LCD from `0x90000000`. Two
  `EXCLUDE_FROM_ALL` diagnostics exist: `qspi-report` (non-destructive ID) and
  `qspi-selftest` (erase/program one scratch sector). Bring-up validated on
  hardware (JEDEC ID `EF 40 18`).
- **microSD** (`src/sd.c`): SD card on **hardware SDMMC2** (CK=PD6/CMD=PD7 AF11;
  D0=PB14/D1=PB15/D2=PB3/D3=PB4 AF9). Bare-metal driver (no HAL): full init
  (CMD0/CMD8/ACMD41/CMD2/CMD3/CMD9/CMD7/ACMD6), 4-bit bus, ~25 MHz, and
  **single + multi-block read *and* write** via FIFO polling (CMD17/18/24/25,
  CMD12 stop, CMD13 busy-poll after writes). Its kernel clock comes from
  **PLL2-R = 100 MHz** (set up by `sd_clock_init()`), deliberately separate from
  PLL1 so `clock.c` is untouched. Diagnostics (`EXCLUDE_FROM_ALL`): `sd-report`
  (read card type/capacity/CID + block 0) and `sd-selftest` (read-and-restore
  write test). H7 gotchas baked in: the SDMMC `WAITRESP` for R3 (ACMD41) must
  ignore CRC; and **`CMDTRANS` auto-starts the DPSM**, so `DCTRL.DTEN` must
  *not* be set manually for data commands. Validated on hardware (read a real
  card's MBR `55 AA`; single + 4-block write/verify/restore all PASS).
- **Memory map** (`linker/STM32H743IITX_FLASH.ld`): code/rodata in **FLASH**
  (`0x08000000`, 2 MB); `.data`/`.bss`/heap/stack in **DTCM** (`0x20000000`,
  128 KB), stack growing down from `_estack`. The **SDRAM at `0xC0000000`** is used
  directly by pointer (no linker region). AXI SRAM, D2/D3 SRAM, ITCM, and NOR
  flash are still unmapped — add `MEMORY` regions when you need them.
- **`include/stm32h743_reg.h`** is hand-written and now covers RCC (incl. PLLs),
  PWR, FLASH, all GPIO ports, FMC SDRAM, LTDC, USART1, and core registers. When
  adding a peripheral, add its registers here (offsets per RM0433), or switch to
  the official CMSIS headers. It also now covers the QUADSPI controller.
  **Watch RCC ENR offsets** — APB3ENR is `0xE4` (LTDC lives there); using `0xE8`
  (APB1LENR) silently leaves the LTDC unclocked.

## Hardware not yet driven

All the on-board peripherals are now driven: clock, SDRAM, LCD, capacitive
touch, QSPI NOR flash, and the microSD card (read **and** write). Remaining
gaps are MCU interfaces with no on-board device wired — **USB / Ethernet /
DCMI** — whose pins are broken out on the headers but are unused.

## Growing the project

- Pin/peripheral assumptions (USART1 on PA9/PA10) live in `src/uart.c`. Changing
  the UART means editing the GPIO AF setup there.
- Adding interrupts → extend the vector table in `src/startup_stm32h743.c`.
- New source files must be added to `add_executable(...)` in `CMakeLists.txt`.
- If the hand-rolled register/HAL approach becomes limiting, the migration path is
  to vendor in STM32CubeH7 (CMSIS + HAL) and replace `stm32h743_reg.h`,
  `startup_*`, and `SystemInit` with the generated equivalents.
