# STM32H743IIT6 Core Board + 7" LCD/Touch — Hardware & Firmware Reference

A portable reference for **another project** that needs to target this board. It
captures the MCU, the clock tree, the memory map, and the exact pin maps /
timings for the on-board **SDRAM**, **RGB LCD (LTDC)**, **capacitive touch
(GT911)**, and **debug UART** — everything that was hard-won from the board
vendor's schematics and examples.

> Source of truth: this repo's bare-metal driver code (`src/*.c`,
> `include/*.h`). Where a value is "load-bearing" (changing it breaks the board)
> it is called out. RM = ST reference manual **RM0433** (STM32H742/743/753).

---

## 1. Board identity

| Item | Value |
|------|-------|
| MCU | **STM32H743IIT6** (Cortex-M7, single core, LQFP176) |
| Core board | `dkm1978/STM32H7IIT6-Core-board` |
| External SDRAM | Winbond **W9825G6KH**, 32 MB, on **FMC bank 1** |
| External flash | Winbond **W25Q128JV** — 16 MB (128 Mbit) **QSPI serial NOR** on QUADSPI bank 1 (driver: `src/qspi.c`) |
| Display | 7" **1024×600 RGB565 IPS** panel on a 40-pin FPC (vendor CN1) |
| Touch | **GT911** capacitive controller (I²C) |
| Programming | **No SWD/ST-Link wired** — ROM bootloader via USB-DFU or UART only |

**Max ratings of the MCU** (for headroom planning): Cortex-M7 up to 480 MHz,
2 MB flash, 1 MB RAM (across AXI/SRAM1-4/TCM), rich peripheral set (USART×4,
UART×4, SPI×6, I²C×4, FDCAN, USB, Ethernet, LTDC, FMC, etc.). This board's
firmware runs the core at **400 MHz** (see clock tree).

---

## 2. Clock tree (as configured by `clock_init()`)

**PLLs are driven from HSI (64 MHz), _not_ the HSE crystal.** The crystal value
is unconfirmed and may be unpopulated; HSI is crystal-independent and proven on
this board. Reproduce this exactly if you reuse the drivers.

```
HSI 64 MHz ──/DIVM1=16──> 4 MHz ──PLL1 (N=200,P=2)──> 800 MHz VCO ──> SYSCLK 400 MHz
           └─/DIVM3=16──> 4 MHz ──PLL3 (N=50, R=8)──> 200 MHz VCO ──> LTDC pixel clk 25 MHz

SYSCLK 400 MHz
  ├─ D1CPRE /1 ──> CPU 400 MHz
  ├─ HPRE   /2 ──> HCLK / AXI / AHB 200 MHz ──> FMC SDRAM clock = HCLK/2 = 100 MHz
  ├─ D1PPRE /2 ──> APB3 100 MHz
  ├─ D2PPRE1/2 ──> APB1 100 MHz
  ├─ D2PPRE2/2 ──> APB2 100 MHz   (but USART1 kernel is forced to HSI — see below)
  └─ D3PPRE /2 ──> APB4 100 MHz   ──> I2C4 kernel = D3PCLK1 (rcc_pclk4) = 100 MHz
```

Key derived clocks a consumer cares about:

| Clock | Frequency | Notes |
|-------|-----------|-------|
| CPU (Cortex-M7) | 400 MHz | VOS Scale 1, 2 flash wait states |
| HCLK / AXI / AHB | 200 MHz | |
| APB1/2/3/4 | 100 MHz | |
| **SDRAM clock** | **100 MHz** | HCLK ÷ 2 (FMC SDCLK = 2) |
| **LTDC pixel clock** | **25 MHz** | PLL3-R; the value this panel actually needs |
| **USART1 kernel** | **64 MHz** | Deliberately pinned to HSI |
| I2C4 kernel | 100 MHz | D3PCLK1 / rcc_pclk4 (reset-default mux) |

**Load-bearing details:**
- `PWR_CR3.SCUEN` **must be cleared** before `VOSRDY` asserts, or voltage Scale 1
  never comes up (the board uses the internal LDO).
- **USART1 is deliberately kept on the HSI kernel clock** so its baud divisor
  (`UART_CLK_HZ = 64 MHz`) stays valid across the bus switch. Don't repoint
  USART1 to a bus clock without changing that constant in `src/uart.c`.
- Flash latency is set to **2 WS** (`LATENCY_2`, `WRHIGHFREQ_2`) **before** the
  switch to PLL1.

---

## 3. Memory map

| Region | Address | Size | Used by this firmware |
|--------|---------|------|-----------------------|
| FLASH | `0x08000000` | 2 MB | code, rodata, `.isr_vector`, init copy of `.data` |
| ITCM | `0x00000000` | 64 KB | *unused* |
| DTCM | `0x20000000` | 128 KB | `.data`, `.bss`, **heap**, **stack** (grows down from `_estack`) |
| AXI SRAM | `0x24000000` | 512 KB | *unused (no linker region yet)* |
| SRAM1/2/3 (D2) | `0x30000000` | 288 KB | *unused* |
| SRAM4 (D3) | `0x38000000` | 64 KB | *unused* |
| **External SDRAM** | **`0xC0000000`** | **32 MB** | **framebuffer + general buffers (by pointer, no linker region)** |
| **QSPI flash (mem-mapped)** | **`0x90000000`** | **16 MB** | W25Q128JV, read-only alias in QUADSPI memory-mapped mode (`qspi_memmap_*`) |

Linker (`linker/STM32H743IITX_FLASH.ld`): only **FLASH** and **DTCM** are
declared as `MEMORY` regions. Heap = 1 KB min, stack = 2 KB min, both in DTCM.
The SDRAM is **not** a linker region — it is reached directly via pointer
(`(volatile uint16_t *)0xC0000000`). To put `.data`/`.bss`/objects in AXI SRAM
or SDRAM, add `MEMORY` + section entries.

---

## 4. External SDRAM — Winbond W9825G6KH (FMC bank 1)

| Property | Value |
|----------|-------|
| Base address | **`0xC0000000`** (FMC SDRAM bank 1) |
| Size | 32 MB (13 row × 9 col × 4 internal banks × 16-bit) |
| Data width | 16-bit (MWID=16) |
| CAS latency | 3 |
| Burst | length 1, sequential, single-location write |
| SDRAM clock | 100 MHz (HCLK/2) |
| Refresh count | `SDRAM_REFRESH = 761` (8192 rows / 64 ms @ 100 MHz) |
| AF for all FMC pins | **AF12**, very-high speed |

**H7 gotcha:** the global **`FMC_BCR1.FMCEN`** must be set or *every* FMC access
hard-faults (unlike F4/F7 where the SDRAM works without it).

**Pin map** (from the vendor's CubeIDE `SDRAM_TEST` project — note **bank 1**,
not bank 2 like the Waveshare reference):

| Signal | Pin(s) |
|--------|--------|
| D0–D1 | PD14, PD15 |
| D2–D3 | PD0, PD1 |
| D4–D12 | PE7–PE15 |
| D13–D15 | PD8, PD9, PD10 |
| A0–A5 | PF0–PF5 |
| A6–A9 | PF12–PF15 |
| A10–A12 | PG0, PG1, PG2 |
| BA0, BA1 | PG4, PG5 |
| NBL0, NBL1 | PE0, PE1 |
| SDCLK | PG8 |
| SDNRAS | PF11 |
| SDNCAS | PG15 |
| SDNWE | PH5 |
| **SDCKE0** | **PH2** (bank-1 specific) |
| **SDNE0** | **PH3** (bank-1 specific) |

> ⚠ **PH2/PH3 are the SDRAM clock-enable / chip-select for bank 1.** Do not
> reuse them for anything else (the Waveshare LTDC examples that use PH2/PH3 for
> LCD signals do **not** apply to this board).

Init: `sdram_init()` configures the pins/controller and runs the JEDEC power-up
sequence. It returns non-zero on controller timeout (never hangs). Must run
**after** `clock_init()` (needs HCLK = 200 MHz). `sdram_selftest(words)` writes
and verifies a unique per-address pattern.

---

## 5. LCD — 1024×600 RGB565 via LTDC

| Property | Value |
|----------|-------|
| Resolution | **1024 × 600** |
| Pixel format | RGB565 (16 bpp) |
| Pixel clock | **25 MHz** (PLL3-R) — load-bearing |
| Framebuffer | single layer 1 at **`0xC0000000`** (base of SDRAM) |
| Backlight enable | **PH6** (driven high) |
| Connector | 40-pin FPC (vendor CN1) |

Only the wired RGB565 bits are connected: **R3–R7, G2–G7, B3–B7** + CLK/DE/HS/VS.

**LTDC pin map** (mostly AF14; two secondary mappings are AF9; B3 is AF13):

| Signal | Pin | AF | | Signal | Pin | AF |
|--------|-----|----|-|--------|-----|----|
| R3 | PH9  | 14 | | G7 | PI2  | 14 |
| R4 | PH10 | 14 | | B3 | PA8  | **13** |
| R5 | PH11 | 14 | | B4 | PG12 | **9** |
| R6 | PH12 | 14 | | B5 | PI5  | 14 |
| R7 | PG6  | 14 | | B6 | PI6  | 14 |
| G2 | PH13 | 14 | | B7 | PI7  | 14 |
| G3 | PG10 | **9** | | DE (DATA EN) | PF10 | 14 |
| G4 | PH15 | 14 | | VSYNC | PI9  | 14 |
| G5 | PI0  | 14 | | HSYNC | PI10 | 14 |
| G6 | PI1  | 14 | | **PIXEL CLK** | **PG7** | 14 |

**Panel timing** (from the board vendor's LTDC example; `lcd_timing_t` is
`{hsw, vsw, hbp, vbp, hfp, vfp}`):

```
HSW = 1    VSW = 3
HBP = 46   VBP = 23
HFP = 40   VFP = 10
Polarities: all sync active-LOW, pixel clock non-inverted (pol = 0)
```

`lcd_set_mode(&timing, pol)` re-programs timing + sync polarity live (useful to
re-tune a different panel). `lcd_init()` brings up pins, the controller, one
full-screen RGB565 layer, and turns on the panel + backlight.

**Framebuffer access:** plain 16-bit writes to SDRAM.
`fb[y * 1024 + x] = rgb565`. Helpers: `lcd_fill(color)`,
`lcd_draw_dot(cx, cy, color)`, `lcd_test_pattern()` (8 color bars). Color macro:
`RGB565(r, g, b)` from 8-bit components.

Layer-1 register specifics already set by `lcd_init()`: `L1PFCR=2` (RGB565),
constant alpha `0xFF`, `L1CFBAR=0xC0000000`, **pitch = 2048 bytes** (1024 px ×
2 B), `L1CFBLNR = 600` lines.

---

## 6. Touch — GT911 capacitive controller (I²C4)

| Property | Value |
|----------|-------|
| Bus | **Hardware I2C4** |
| SCL / SDA | **PD12 / PD13**, AF4, open-drain |
| Pull-ups | external on board (no internal pull used) |
| I²C address | **0x5D** (7-bit) |
| RST | **PD11** |
| INT | **PH7** |
| Bus speed | ~85–100 kHz (`TIMINGR = 0x307075B1` @ 100 MHz kernel) |
| Coordinate space | panel pixels (0–1023 × 0–599) |

**Reset / address-select sequence** (`touch_init()`): hold RST low with INT low,
release RST (INT low selects address **0x5D**), then release INT as a floating
input, wait ~100 ms for boot. The driver reads the product ID at `0x8140` to
confirm presence.

**Register map used:**

| Register | Address | Purpose |
|----------|---------|---------|
| Product ID | `0x8140` | 4 ASCII bytes (e.g. "911") |
| Status / count | `0x814E` | bit7 = data ready, bits[3:0] = #points; **write 0 to clear** |
| Touch point 1 | `0x8150` | X lo/hi, Y lo/hi (little-endian within the 4 bytes) |

`touch_read(&x, &y)` returns the first touch point (1 = touched). It only reads
point 1; extend for multi-touch by reading further point blocks (`0x8158`, …).

**I²C driver** (`src/i2c.c`): bare register-level master, blocking, AUTOEND with
hardware STOP. Functions: `i2c4_init()`, `i2c4_write(addr7, buf, len)`,
`i2c4_read(addr7, buf, len)` — usable for any I²C4 device, not just the GT911.

---

## 7. Debug UART — USART1

| Property | Value |
|----------|-------|
| Peripheral | **USART1** |
| TX / RX | **PA9 / PA10**, AF7 |
| Baud / framing | **115200 8N1** |
| Kernel clock | **64 MHz (HSI)** — pinned, do not change without updating `UART_CLK_HZ` |

`printf` is routed to the UART via `_write()` in `src/syscalls.c` →
`uart_write()`; `main` sets `setvbuf(stdout, NULL, _IONBF, 0)` (unbuffered).
`_sbrk()` provides the heap (from the linker `_end`) for malloc/printf.

> Note: the **app** uses **8N1**. The **ROM UART bootloader** (for flashing) uses
> **8E1** — different framing, same PA9/PA10 pins.

---

## 8. External QSPI NOR flash — Winbond W25Q128JV (driver: `src/qspi.c`)

The on-board "NOR flash" is a **Winbond W25Q128JVSQ** serial flash — **16 MB
(128 Mbit)**, standard / dual / quad SPI, up to 133 MHz. It is **not** a parallel
NOR on the FMC; it hangs off **QUADSPI bank 1**. It is driven by `src/qspi.c`
(single-line indirect at ~25 MHz: ID/status, write-enable/erase/program/read,
and memory-mapped reads); `src/flash_image.c` is an end-to-end usage example.

| QSPI signal | STM32 pin | AF | Flash pin |
|-------------|-----------|----|-----------|
| CLK | **PB2** | AF9 | CLK (6) |
| NCS | **PB6** | AF10 | /CS (1) — 4.7 kΩ pull-up (R4) |
| IO0 (DI) | **PF8** | AF10 | DI/IO0 (5) |
| IO1 (DO) | **PF9** | AF10 | DO/IO1 (2) |
| IO2 (/WP) | **PF7** | AF9 | IO2 (3) |
| IO3 (/HOLD) | **PF6** | AF9 | IO3 (7) |

- **Memory-mapped base:** `0x90000000` (only when QUADSPI is put in
  memory-mapped mode; in indirect mode you read/write via the QUADSPI data
  register). FSIZE = 23 (2^24 = 16 MB).
- **JEDEC ID** (command `0x9F`) for a healthy part: **`EF 40 18`**
  (Winbond / type 0x40 / 16 MB). Read this first to validate wiring
  non-destructively.
- **Common commands:** `0x06` Write Enable, `0x05` Read Status (BUSY = bit0),
  `0x20` Sector Erase 4 KB, `0x02` Page Program 256 B, `0x03` Read,
  `0x6B` Fast Read Quad Output.
- **Bring-up notes:** none of these pins clash with the SDRAM/LTDC/touch/UART,
  and the polling indirect-mode driver needs **no vector-table change**. The
  QUADSPI register defs live in `include/stm32h743_reg.h`; the driver is
  `src/qspi.c`. Validated on hardware (JEDEC ID `EF 40 18`, full erase/program/
  read self-test passed).
- **Flashing caveat:** because this board has **no SWD**, STM32CubeProgrammer's
  external-loader route (which needs a debug connection) is **not** available —
  validate/use the flash from firmware, not from the programmer.

---

## 9. microSD card — SDMMC2 (`src/sd.c`)

The on-board microSD slot is wired to **hardware SDMMC2**. Driven by `src/sd.c`
(bare-metal, no HAL): full SD init, 4-bit bus, ~25 MHz, **single + multi-block
read and write**.

| Signal | STM32 pin | AF |
|--------|-----------|----|
| CK | **PD6** | AF11 |
| CMD | **PD7** | AF11 |
| D0 | **PB14** | AF9 |
| D1 | **PB15** | AF9 |
| D2 | **PB3** | AF9 |
| D3 | **PB4** | AF9 |

- **Kernel clock:** **PLL2-R = 100 MHz** (HSI/16 → 4 MHz, ×50 → 200 MHz VCO,
  /2). Set up by `sd_clock_init()`, kept separate from PLL1 so `clock.c` stays
  untouched. SDMMC bus clock = kernel / (2·CLKDIV): 400 kHz for init
  (CLKDIV=125), 25 MHz for transfer (CLKDIV=2). `SDMMCSEL` (D1CCIPR) = pll2_r.
- **Init sequence:** CMD0 → CMD8 → ACMD41(HCS) → CMD2 (CID) → CMD3 (RCA) →
  CMD9 (CSD/capacity) → CMD7 (select) → ACMD6 (4-bit) → CMD16 (512 B).
- **Transfers:** single read/write (CMD17/CMD24) and multi-block read/write
  (CMD18/CMD25 with a CMD12 stop). Writes busy-poll with CMD13 until the card
  leaves the programming state. API: `sd_read_block` / `sd_write_block` /
  `sd_read_blocks` / `sd_write_blocks`.
- **H7 SDMMC gotchas (baked into the driver):** R3 responses (ACMD41) carry no
  valid CRC, so `CCRCFAIL` must be treated as success there; and **`CMDTRANS`
  auto-starts the DPSM** on the H7 IP, so `DCTRL.DTEN` must *not* be set manually
  for a data command (doing so jams the command path). Hardware flow control
  (`HWFC_EN`) is on to avoid FIFO over/under-runs.
- **Diagnostics:** `sd-report` (read type/capacity/CID + block 0, `55 AA` check)
  and `sd-selftest` (read-and-restore single + 4-block write test). Both
  `EXCLUDE_FROM_ALL`; validated on hardware against a real ~8 GB SDHC card.
- **Filesystem:** ChaN's **FatFs R0.15** is vendored on top of this driver
  (`third_party/fatfs/` + `src/fatfs_diskio.c`); code page 437, long filenames,
  read-write. Diagnostic `fatfs-report` mounts the card and lists the root dir.
  See `third_party/fatfs/README.md`.
- **Not yet:** high speed / SDR50/104, CMD23 predefined block count, SDIO.

---

## 10. Programming / flashing (no SWD on this board)

Enter the ROM bootloader with **BOOT0 high + RESET**, then:

```sh
# USB DFU (board's native USB in DFU mode)
STM32_Programmer_CLI -c port=USB1 -w firmware.hex

# UART system bootloader (8E1!) on PA9/PA10 via a USB-TTL adapter
STM32_Programmer_CLI -c port=COMx br=115200 -w firmware.hex
```

After flashing, **press RESET with BOOT0 low** to run the app. The
bootloader's "go"/DFU-detach does not reliably start the app. The `-rst` command
is SWD-only and errors over UART/DFU — ignore it.

---

## 11. Pin allocation summary (what's already taken)

If you add peripherals, avoid these. (Ports A,D,E,F,G,H,I are heavily used.)

| Pins | Owner |
|------|-------|
| PA8 | LCD B3 |
| PA9, PA10 | USART1 TX/RX (debug) |
| PB2, PB6 | QSPI flash CLK / NCS |
| PB3, PB4, PB14, PB15 | SDMMC2 D2 / D3 / D0 / D1 (microSD) |
| PD0, PD1, PD8, PD9, PD10, PD14, PD15 | SDRAM data |
| PD6, PD7 | SDMMC2 CK / CMD (microSD) |
| PD11 | GT911 RST |
| PD12, PD13 | I2C4 SCL/SDA (touch) |
| PE0, PE1, PE7–PE15 | SDRAM NBL/data |
| PF0–PF5, PF11, PF12–PF15 | SDRAM addr/RAS |
| PF6, PF7, PF8, PF9 | QSPI flash IO3 / IO2 / IO0 / IO1 |
| PF10 | LCD DE |
| PG0, PG1, PG2, PG4, PG5, PG8, PG15 | SDRAM addr/BA/clk/CAS |
| PG6, PG7, PG10, PG12 | LCD R7 / PIXCLK / G3 / B4 |
| PH2, PH3, PH5 | SDRAM CKE/NE/WE (bank 1) |
| PH6 | LCD backlight enable |
| PH7 | GT911 INT |
| PH9–PH13, PH15 | LCD R/G data |
| PI0–PI2, PI5–PI7, PI9, PI10 | LCD G/B data, V/HSYNC |

**Free / lightly-used ports** good for new peripherals: most of **port A**
(except PA8–PA10), **port B** (except PB2/PB3/PB4/PB6/PB14/PB15, used by
QSPI/SDMMC2), **port C**, and the **UARTs/SPIs/CAN** on pins not listed above.
Note **PLL2** is now consumed by the SDMMC kernel clock; **PLL3** by the LTDC. For a new serial link (e.g. Modbus RS-485), USART2/3,
UART4/5/7/8 are all unused — pick pins outside the table above and remember to
add their register defs to `include/stm32h743_reg.h` and (if using interrupts)
extend the vector table in `src/startup_stm32h743.c`.

---

## 12. Firmware integration notes (bare-metal, no HAL)

This project is deliberately **self-contained**: no CubeMX/HAL/CMSIS/SDK. It
ships its own minimal register header, C startup/vector table, and linker script.

- **Vector table** (`src/startup_stm32h743.c`): defines **only the 16 core
  exceptions**; peripheral IRQ vectors are omitted. **Enabling any peripheral
  interrupt requires extending `g_vectors`**, or the handler falls off the end.
- **`SystemInit`** (runs before `main`): enables the **FPU** (`CPACR`) — required
  because the build uses `-mfloat-abi=hard`; sets **`VTOR = 0x08000000`**; forces
  **HSI = 64 MHz**. `main` then calls `clock_init()`.
- **Register header** (`include/stm32h743_reg.h`) is hand-written: covers RCC
  (incl. PLLs), PWR, FLASH, all GPIO ports, FMC SDRAM, LTDC, USART1, I2C4, core.
  Add registers per RM0433 as you grow. **Watch RCC ENR offsets** — APB3ENR is
  `0xE4` (LTDC lives there); using `0xE8` silently leaves the LTDC unclocked.
- **GPIO helpers** (`gpio_af.h`): `gpio_init_af(base, pin, af, speed)`,
  `gpio_init_af_od(...)` (open-drain), `gpio_init_output(base, pin)`.
- **Build**: CMake + Ninja + `arm-none-eabi-gcc`, toolchain file
  `cmake/arm-none-eabi-gcc.cmake` (mandatory). New `.c` files must be added to
  `add_executable(...)` in `CMakeLists.txt`. No tests, no linter.

**Boot order that a reusing project should follow:**

```c
uart_init();                 // 64 MHz HSI default — survives the clock switch
clock_init();                // -> 400 MHz, SDRAM clk 100 MHz, LTDC pixclk 25 MHz
sdram_init();                // 32 MB @ 0xC0000000 (needs HCLK 200 MHz)
lcd_init();                  // LTDC + panel + backlight (framebuffer in SDRAM)
touch_init();                // GT911 on I2C4
```

---

## 13. Quick-reference constants

```c
#define SDRAM_BASE    0xC0000000U   // 32 MB external SDRAM
#define LCD_WIDTH     1024U
#define LCD_HEIGHT    600U
// Framebuffer pixel:  ((volatile uint16_t*)0xC0000000)[y*1024 + x] = RGB565(r,g,b)

// Clocks
CPU      = 400 MHz     HCLK = 200 MHz     APBx = 100 MHz
SDRAM    = 100 MHz     LTDC pixclk = 25 MHz
USART1 kernel = 64 MHz (HSI)            I2C4 kernel = 100 MHz

// Debug serial: USART1, PA9(TX)/PA10(RX), 115200 8N1
// Touch:        GT911 @ I2C4 0x5D, SCL=PD12 SDA=PD13, RST=PD11 INT=PH7
// QSPI flash:   W25Q128JV, CLK=PB2 NCS=PB6 IO0=PF8 IO1=PF9 IO2=PF7 IO3=PF6
// microSD:      SDMMC2, CK=PD6 CMD=PD7 D0=PB14 D1=PB15 D2=PB3 D3=PB4 (ker=PLL2-R 100MHz)
```
