cmake_minimum_required(VERSION 3.20)

# The toolchain file must be supplied on the command line:
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
project(stm32h743-lcd-touch C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TARGET ${PROJECT_NAME})
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/STM32H743IITX_FLASH.ld)

# Cortex-M7 with double-precision hardware FPU.
set(CPU_FLAGS -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard)

add_executable(${TARGET}
    src/startup_stm32h743.c
    src/syscalls.c
    src/uart.c
    src/clock.c
    src/sdram.c
    src/lcd.c
    src/i2c.c
    src/touch.c
    src/main.c
)

target_include_directories(${TARGET} PRIVATE include)

target_compile_options(${TARGET} PRIVATE
    ${CPU_FLAGS}
    -Wall -Wextra
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)

target_link_options(${TARGET} PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=${TARGET}.map
    -Wl,--print-memory-usage
)

# Make the linker re-run if the script changes.
set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

# Produce .hex and .bin alongside the .elf and print section sizes.
add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET}> ${TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
    COMMENT "Generating ${TARGET}.hex and ${TARGET}.bin"
)
