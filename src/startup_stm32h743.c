/*
 * Startup / vector table for STM32H743 (Cortex-M7), written in C.
 *
 * Only the 16 core exception vectors are defined here. We do not enable any
 * peripheral interrupts, so the (much longer) IRQ portion of the table is
 * intentionally omitted. Add it if/when you start using interrupts.
 */
#include <stdint.h>
#include "stm32h743_reg.h"

/* Symbols provided by the linker script. */
extern uint32_t _sidata; /* start of .data init values in FLASH */
extern uint32_t _sdata;  /* start of .data in RAM               */
extern uint32_t _edata;  /* end of .data in RAM                 */
extern uint32_t _sbss;   /* start of .bss in RAM                */
extern uint32_t _ebss;   /* end of .bss in RAM                  */
extern uint32_t _estack; /* top of stack                        */

int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Core exceptions are weak aliases to Default_Handler so they can be overridden
 * elsewhere without editing this file. */
#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void)        WEAK_ALIAS;
void HardFault_Handler(void)  WEAK_ALIAS;
void MemManage_Handler(void)  WEAK_ALIAS;
void BusFault_Handler(void)   WEAK_ALIAS;
void UsageFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void)        WEAK_ALIAS;
void DebugMon_Handler(void)   WEAK_ALIAS;
void PendSV_Handler(void)     WEAK_ALIAS;
void SysTick_Handler(void)    WEAK_ALIAS;

typedef void (*vector_t)(void);

__attribute__((section(".isr_vector"), used))
const vector_t g_vectors[] = {
    (vector_t)(&_estack),   /* 0  Initial stack pointer */
    Reset_Handler,          /* 1  Reset                 */
    NMI_Handler,            /* 2                        */
    HardFault_Handler,      /* 3                        */
    MemManage_Handler,      /* 4                        */
    BusFault_Handler,       /* 5                        */
    UsageFault_Handler,     /* 6                        */
    0, 0, 0, 0,             /* 7-10 reserved            */
    SVC_Handler,            /* 11                       */
    DebugMon_Handler,       /* 12                       */
    0,                      /* 13 reserved              */
    PendSV_Handler,         /* 14                       */
    SysTick_Handler,        /* 15                       */
};

/* Bring the chip into a known state before main(): enable the FPU, point VTOR
 * at our vector table, and force HSI = 64 MHz (HSIDIV = /1). */
static void SystemInit(void)
{
    /* Full access to CP10 and CP11 (the FPU). Cortex-M7 has a double-precision
     * FPU and we build with -mfloat-abi=hard, so this must happen before any
     * floating-point instruction executes. */
    SCB_CPACR |= (0xFU << 20);
    __asm volatile("dsb");
    __asm volatile("isb");

    /* Vector table lives at the start of FLASH. */
    SCB_VTOR = 0x08000000UL;

    /* Make sure HSI is on and undivided (64 MHz). It is the reset default, but
     * we set it explicitly so the UART baud-rate math is guaranteed correct. */
    RCC_CR |= RCC_CR_HSION;
    while (!(RCC_CR & RCC_CR_HSIRDY)) { }
    RCC_CR &= ~RCC_CR_HSIDIV_Msk;
}

void Reset_Handler(void)
{
    /* Copy initialized data from FLASH to RAM. */
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; ) {
        *dst++ = *src++;
    }
    /* Zero the .bss section. */
    for (uint32_t *dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0;
    }

    SystemInit();
    main();

    for (;;) { } /* main() should never return */
}

void Default_Handler(void)
{
    for (;;) { } /* park here so a fault is catchable in the debugger */
}
