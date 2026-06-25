<#
.SYNOPSIS
  Bundle the portable bare-metal STM32H743 drivers into a single zip another
  project can drop in.

.DESCRIPTION
  Collects the reusable driver/platform files plus the hardware docs into
  dist/stm32h743-h7-drivers.zip, preserving the src/ include/ linker/ cmake/
  docs/ test/ layout. Excludes the app entry point (src/main.c) and build output.

.PARAMETER IncludeExamples
  Also include the demo (src/flash_image.*) and the test/*-report / *-selftest
  diagnostics as usage examples. Default: on.

.EXAMPLE
  pwsh tools/package-drivers.ps1
  pwsh tools/package-drivers.ps1 -IncludeExamples:$false
#>
param(
    [bool]$IncludeExamples = $true
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot          # repo root (tools/..)
$stage = Join-Path $root 'dist\stm32h743-h7-drivers'
$zip   = Join-Path $root 'dist\stm32h743-h7-drivers.zip'

# --- Portable file set (relative to repo root) ---------------------------------
$core = @(
    'include/stm32h743_reg.h',
    'include/gpio_af.h',
    'src/startup_stm32h743.c',
    'src/syscalls.c',
    'linker/STM32H743IITX_FLASH.ld',
    'cmake/arm-none-eabi-gcc.cmake'
)
$drivers = @(
    'src/clock.c',  'include/clock.h',
    'src/uart.c',   'include/uart.h',
    'src/sdram.c',  'include/sdram.h',
    'src/lcd.c',    'include/lcd.h',
    'src/i2c.c',    'include/i2c.h',
    'src/touch.c',  'include/touch.h',
    'src/qspi.c',   'include/qspi.h',
    'src/sd.c',     'include/sd.h'
)
$docs = @(
    'docs/BOARD_REFERENCE.md',
    'docs/PORTING.md'
)
$examples = @(
    'src/flash_image.c', 'include/flash_image.h',
    'test/qspi_report.c', 'test/qspi_selftest.c',
    'test/sd_report.c',   'test/sd_selftest.c',
    'test/clock_report.c'
)

$files = $core + $drivers + $docs
if ($IncludeExamples) { $files += $examples }

# --- Stage (clean copy preserving relative paths) ------------------------------
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

$missing = @()
foreach ($rel in $files) {
    $srcPath = Join-Path $root $rel
    if (-not (Test-Path $srcPath)) { $missing += $rel; continue }
    $dstPath = Join-Path $stage $rel
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dstPath) | Out-Null
    Copy-Item $srcPath $dstPath
}
if ($missing.Count) {
    Write-Warning "Skipped missing files:`n  $($missing -join "`n  ")"
}

# PORTING.md is the bundle's entry point; surface it at the top level too.
Copy-Item (Join-Path $root 'docs/PORTING.md') (Join-Path $stage 'README.md')

# --- Zip -----------------------------------------------------------------------
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip

$count = (Get-ChildItem -Recurse -File $stage | Measure-Object).Count
$kb    = [math]::Round((Get-Item $zip).Length / 1KB, 1)
Write-Host ""
Write-Host "Packaged $count files -> $zip ($kb KB)" -ForegroundColor Green
Write-Host "Staged tree at         $stage"
Write-Host "Start from PORTING.md (also copied to the bundle root as README.md)."
