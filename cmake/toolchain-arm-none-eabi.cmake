# Toolchain file for the QEMU Stellaris LM3S6965 (ARM Cortex-M3) firmware.
# Pass this file during the first CMake configure step:
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER arm-none-eabi-gcc CACHE FILEPATH "ARM bare-metal C compiler")
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc CACHE FILEPATH "ARM bare-metal ASM compiler")

set(_ARM_CPU_FLAGS "-mcpu=cortex-m3 -mthumb")
set(CMAKE_C_FLAGS_INIT "${_ARM_CPU_FLAGS} -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -DSTACK_SIZE=2048")
set(CMAKE_ASM_FLAGS_INIT "${_ARM_CPU_FLAGS}")

get_filename_component(_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(_PROJECT_ROOT "${_TOOLCHAIN_DIR}/.." ABSOLUTE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_ARM_CPU_FLAGS} -T ${_PROJECT_ROOT}/standalone.ld --specs=nosys.specs --specs=nano.specs -nostartfiles")

unset(_PROJECT_ROOT)
unset(_TOOLCHAIN_DIR)
unset(_ARM_CPU_FLAGS)
