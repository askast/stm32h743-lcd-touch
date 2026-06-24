# CMake toolchain file for the GNU Arm Embedded toolchain (arm-none-eabi-gcc).
# Pass with: -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Don't try to link a test executable during compiler detection (no host libc).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "")
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size    CACHE INTERNAL "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
