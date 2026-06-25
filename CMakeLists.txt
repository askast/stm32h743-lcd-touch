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
    src/qspi.c
    src/flash_image.c
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

# --- Optional clock diagnostic firmware ------------------------------------
# Reports + measures the clock tree over UART. Not built by default:
#   cmake --build build --target clock-report
add_executable(clock-report EXCLUDE_FROM_ALL
    src/startup_stm32h743.c
    src/syscalls.c
    src/uart.c
    src/clock.c
    test/clock_report.c
)
target_include_directories(clock-report PRIVATE include)
target_compile_options(clock-report PRIVATE
    ${CPU_FLAGS}
    -Wall -Wextra
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)
target_link_options(clock-report PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=clock-report.map
    -Wl,--print-memory-usage
)
set_target_properties(clock-report PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})
add_custom_command(TARGET clock-report POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:clock-report> clock-report.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:clock-report>
    COMMENT "Generating clock-report.hex"
)

# --- Optional QSPI flash diagnostic ----------------------------------------
# Non-destructively reads the W25Q128JV JEDEC ID over UART. Not built by default:
#   cmake --build build --target qspi-report
add_executable(qspi-report EXCLUDE_FROM_ALL
    src/startup_stm32h743.c
    src/syscalls.c
    src/uart.c
    src/clock.c
    src/qspi.c
    test/qspi_report.c
)
target_include_directories(qspi-report PRIVATE include)
target_compile_options(qspi-report PRIVATE
    ${CPU_FLAGS}
    -Wall -Wextra
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)
target_link_options(qspi-report PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=qspi-report.map
    -Wl,--print-memory-usage
)
set_target_properties(qspi-report PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})
add_custom_command(TARGET qspi-report POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:qspi-report> qspi-report.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:qspi-report>
    COMMENT "Generating qspi-report.hex"
)

# --- Optional QSPI flash SELF-TEST (destructive) ---------------------------
# Erases+reprograms ONE 4KB sector to verify the full data path. Not by default:
#   cmake --build build --target qspi-selftest
add_executable(qspi-selftest EXCLUDE_FROM_ALL
    src/startup_stm32h743.c
    src/syscalls.c
    src/uart.c
    src/clock.c
    src/qspi.c
    test/qspi_selftest.c
)
target_include_directories(qspi-selftest PRIVATE include)
target_compile_options(qspi-selftest PRIVATE
    ${CPU_FLAGS}
    -Wall -Wextra
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)
target_link_options(qspi-selftest PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=qspi-selftest.map
    -Wl,--print-memory-usage
)
set_target_properties(qspi-selftest PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})
add_custom_command(TARGET qspi-selftest POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:qspi-selftest> qspi-selftest.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:qspi-selftest>
    COMMENT "Generating qspi-selftest.hex"
)

# --- Optional microSD (SDMMC2) diagnostic ----------------------------------
# Read-only: init the card, report type/capacity/CID, read block 0. Not default:
#   cmake --build build --target sd-report
add_executable(sd-report EXCLUDE_FROM_ALL
    src/startup_stm32h743.c
    src/syscalls.c
    src/uart.c
    src/clock.c
    src/sd.c
    test/sd_report.c
)
target_include_directories(sd-report PRIVATE include)
target_compile_options(sd-report PRIVATE
    ${CPU_FLAGS}
    -Wall -Wextra
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)
target_link_options(sd-report PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=sd-report.map
    -Wl,--print-memory-usage
)
set_target_properties(sd-report PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})
add_custom_command(TARGET sd-report POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:sd-report> sd-report.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:sd-report>
    COMMENT "Generating sd-report.hex"
)

# --- Optional microSD WRITE self-test (read-and-restore) -------------------
# Validates write + multi-block by writing a pattern and restoring the original:
#   cmake --build build --target sd-selftest
add_executable(sd-selftest EXCLUDE_FROM_ALL
    src/startup_stm32h743.c
    src/syscalls.c
    src/uart.c
    src/clock.c
    src/sd.c
    test/sd_selftest.c
)
target_include_directories(sd-selftest PRIVATE include)
target_compile_options(sd-selftest PRIVATE
    ${CPU_FLAGS}
    -Wall -Wextra
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2>
)
target_link_options(sd-selftest PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=sd-selftest.map
    -Wl,--print-memory-usage
)
set_target_properties(sd-selftest PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})
add_custom_command(TARGET sd-selftest POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:sd-selftest> sd-selftest.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:sd-selftest>
    COMMENT "Generating sd-selftest.hex"
)
