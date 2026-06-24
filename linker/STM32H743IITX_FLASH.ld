/*
 * Linker script for STM32H743IIT6.
 *   FLASH : 2 MB @ 0x08000000
 *   DTCM  : 128 KB @ 0x20000000  (used here for .data/.bss/heap/stack)
 *
 * Other memories on this part (AXI SRAM @ 0x24000000, D2/D3 SRAM, ITCM, the
 * external 32 MB SDRAM @ 0xC0000000, NOR flash) are not used by this minimal
 * project. Add MEMORY regions + sections for them as the project grows.
 */
ENTRY(Reset_Handler)

_estack    = ORIGIN(DTCM) + LENGTH(DTCM);  /* top of stack (DTCM grows down) */
_Min_Heap_Size  = 0x400;   /* 1 KB heap  */
_Min_Stack_Size = 0x800;   /* 2 KB stack */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    DTCM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* ARM stack-unwinding tables (kept minimal but present). */
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* C++ static constructors/destructors. */
    .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } >FLASH
    .init_array    : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } >FLASH
    .fini_array    : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } >FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >DTCM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >DTCM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >DTCM

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
