# FatFs (vendored)

Generic FAT/exFAT filesystem module by ChaN — http://elm-chan.org/fsw/ff/

- **Version:** R0.15 (source archive `ff15.zip`).
- **License:** ChaN's 1-clause BSD-style license, reproduced at the top of
  `ff.h` / `ff.c`. Unmodified from upstream except `ffconf.h` (see below).
- **Files kept:** `ff.c`, `ff.h`, `ffconf.h`, `diskio.h`, `ffunicode.c` (for
  LFN). The upstream `diskio.c` template, `ffsystem.c`, and the sample apps are
  **not** vendored — we don't need them for this configuration. (`ffunicode.c`
  is ~2 MB of OEM/Unicode tables, but compiles down to just the selected code
  page.)

## Local configuration (`ffconf.h` changes from upstream defaults)

| Option | Default | Here | Why |
|--------|---------|------|-----|
| `FF_CODE_PAGE` | 932 | **437** | US; single-byte OEM code page |
| `FF_USE_STRFUNC` | 0 | **1** | enable `f_gets()` / `f_puts()` / `f_printf()` |
| `FF_USE_LFN` | 0 | **2** | long filenames; work buffer on the stack (needs `ffunicode.c`) |

Everything else is upstream default: read-write (`FF_FS_READONLY 0`), one
volume, fixed 512-byte sectors. LFN uses a `(FF_MAX_LFN+1)*2 = 512`-byte stack
buffer per call — fine given the stack lives in 128 KB DTCM.

## How it connects

`ff.c` calls the `disk_*` functions declared in `diskio.h`; those are
implemented in **`src/fatfs_diskio.c`**, which forwards them to the bare-metal
SD driver in `src/sd.c` (`sd_init` / `sd_read_blocks` / `sd_write_blocks`). A
fixed `get_fattime()` lives there too (no RTC on this board).

## Upgrading

- **Formatting:** set `FF_USE_MKFS 1` to enable `f_mkfs()`.
- **exFAT:** set `FF_FS_EXFAT 1` (also requires LFN, already on).
- **New FatFs release:** re-copy the four files, then re-apply the two `ffconf.h`
  changes above.
