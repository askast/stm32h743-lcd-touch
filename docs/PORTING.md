# Porting these drivers into another STM32H743 project

These are **self-contained, bare-metal** drivers — no CubeMX, HAL, CMSIS, or
vendor SDK. Each is a small `.c`/`.h` pair that talks to the hardware through one
hand-written register header. You can lift the whole set, or cherry-pick a single
driver and the few files it depends on.

> ⚠ **Board-specific pin maps.** The GPIO/AF assignments in these drivers match
> *this* board (`dkm1978/STM32H7IIT6-Core-board`). On different wiring you must
> edit the pin tables — see [`BOARD_REFERENCE.md`](BOARD_REFERENCE.md) for the
> exact maps each driver uses. Register offsets are generic STM32H743 (RM0433).

## Dependency layers

```
stm32h743_reg.h        <- foundation: every file includes it
gpio_af.h              <- inline GPIO AF/output helpers (needs reg)
startup + linker + cmake + syscalls   <- the platform base (runtime, printf, build)
clock.c                <- 400 MHz tree; SDRAM/QSPI/SDMMC assume it has run
uart.c (+ syscalls.c)  <- debug printf over USART1
   |
   +-- sdram.c   (reg, gpio_af, clock)
   +-- lcd.c     (reg, gpio_af, sdram)        <- framebuffer lives in SDRAM
   +-- i2c.c     (reg, gpio_af)
   +-- touch.c   (i2c, gpio_af, clock, printf)
   +-- qspi.c    (reg, gpio_af; assumes clock_init for the 25 MHz QSPI clock)
   +-- sd.c      (reg, gpio_af, clock; sets up its own PLL2 kernel clock)
```

## File manifest

| Module | Files | Needs |
|--------|-------|-------|
| **Register defs** | `include/stm32h743_reg.h` | — |
| **GPIO helpers** | `include/gpio_af.h` | reg |
| **Platform base** | `src/startup_stm32h743.c`, `linker/STM32H743IITX_FLASH.ld`, `cmake/arm-none-eabi-gcc.cmake`, `src/syscalls.c` | reg |
| **Clock** | `src/clock.c`, `include/clock.h` | reg |
| **UART + printf** | `src/uart.c`, `include/uart.h`, `src/syscalls.c` | reg |
| **SDRAM** | `src/sdram.c`, `include/sdram.h` | reg, gpio_af, clock |
| **LCD (LTDC)** | `src/lcd.c`, `include/lcd.h` | reg, gpio_af, sdram |
| **Touch (GT911)** | `src/touch.c`, `include/touch.h`, `src/i2c.c`, `include/i2c.h` | reg, gpio_af, clock, uart |
| **QSPI NOR flash** | `src/qspi.c`, `include/qspi.h` | reg, gpio_af, clock |
| **microSD (SDMMC2)** | `src/sd.c`, `include/sd.h` | reg, gpio_af, clock |
| *Example* | `src/flash_image.c`, `include/flash_image.h`, `test/*_report.c`, `test/*_selftest.c` | the modules they exercise |

## Integration steps

1. **Copy the files** you need (keep the `src/`/`include/` split, or flatten —
   just fix include paths). Add the `.c` files to your build's source list and
   put `include/` on the compiler search path.

2. **Provide `SystemInit`** (in `src/startup_stm32h743.c`). It must run before
   `main` and does three load-bearing things: enable the **FPU** (`CPACR`) —
   required because the build is `-mfloat-abi=hard`; set **`VTOR`** to the vector
   table; and force **HSI = 64 MHz**. If you instead use a vendor CMSIS startup,
   replicate those three before calling driver code.

3. **Build flags** (Cortex-M7 + double-precision FPU):
   `-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard`, link with
   `--specs=nano.specs`. Use the provided **linker script** (code in FLASH,
   data/heap/stack in DTCM; the external SDRAM `0xC0000000` and the QSPI
   mem-mapped region `0x90000000` are reached by pointer, not linker regions).

4. **Call `clock_init()` early** in `main`. It raises the chip to 400 MHz SYSCLK
   / 200 MHz HCLK and sets PLL3-R for the LTDC. SDRAM (100 MHz FMC clock) and
   QSPI (25 MHz) assume this has run. `uart_init()` works before *and* after the
   switch because USART1 stays on the 64 MHz HSI kernel clock.

5. **Init the drivers you use** (typical order):
   ```c
   uart_init();                 // USART1, 115200 8N1 — printf works
   clock_init();                // 400 MHz; SDRAM/QSPI depend on this
   sdram_init();                // 32 MB @ 0xC0000000 (framebuffer)
   lcd_init();                  // LTDC + panel + backlight
   touch_init();                // GT911 on I2C4
   qspi_init();                 // W25Q128JV on QUADSPI
   sd_clock_init(); sd_init(&card);   // SDMMC2 (PLL2-R kernel clock)
   ```

6. **Interrupts:** the vector table in `src/startup_stm32h743.c` defines only the
   16 core exceptions. All drivers here are **polling** (no IRQs), so nothing
   extra is needed — but if you add interrupt-driven code, extend `g_vectors`
   first or the handler runs off the end of the table.

## Per-driver quickstart

```c
/* QSPI NOR flash (16 MB) */
qspi_init();
uint8_t id[3]; qspi_read_jedec_id(id);          // -> EF 40 18
qspi_erase_sector(addr);
qspi_program_buffer(addr, data, len);            // page-split program
qspi_read(addr, buf, len);                        // indirect read
uint8_t b[256]; qspi_memmap_read(addr, b, 256);  // or read 0x90000000 by pointer

/* microSD (SDMMC2) */
sd_clock_init();
sd_card_t card; sd_init(&card);                  // type, capacity, RCA, CID
sd_read_block(lba, blk512);
sd_write_block(lba, blk512);
sd_read_blocks(lba, buf, count);                  // CMD18
sd_write_blocks(lba, buf, count);                 // CMD25

/* LCD */
lcd_init();
lcd_fill(RGB565(0,0,0));
lcd_blit(x, y, w, h, pixels);                     // RGB565 bitmap -> framebuffer
```

## Clock budget if you take everything

- **PLL1** → 400 MHz SYSCLK / 200 MHz HCLK (core + buses + FMC SDRAM clock).
- **PLL2-R** → 100 MHz, consumed by the **SDMMC** kernel clock (`sd_clock_init`).
- **PLL3-R** → 25 MHz, the **LTDC** pixel clock.

So PLL2 and PLL3 are spoken for once you pull in SD and LCD; PLL1-Q is still free
if you need another kernel clock.

See [`BOARD_REFERENCE.md`](BOARD_REFERENCE.md) for the full per-peripheral pin
maps, timings, and H7 gotchas.
