cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Please configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake so CMake detects arm-none-eabi-gcc before project().")
endif()

# FreeRTOS dependency. setup.sh downloads the upstream repository into FreeRTOS/.
set(FREERTOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS" CACHE PATH "Path to the upstream FreeRTOS repository checkout")
set(FREERTOS_DIR "${FREERTOS_ROOT}/FreeRTOS")
set(FREERTOS_SRC_DIR "${FREERTOS_DIR}/Source")

if(NOT EXISTS "${FREERTOS_SRC_DIR}/tasks.c")
    message(FATAL_ERROR
        "FreeRTOS kernel sources were not found at:\n"
        "  ${FREERTOS_SRC_DIR}/tasks.c\n\n"
        "Run ./setup.sh to download the dependency, or initialize/update the FreeRTOS checkout manually. "
        "If you keep FreeRTOS outside this repository, configure with -DFREERTOS_ROOT=/path/to/FreeRTOS.")
endif()

project(RTOSDemo C ASM)

# 1. 我们的项目源文件
set(PROJECT_SOURCES
    src/main.c
    src/bsp/bsp_uart.c
    src/bsp/startup.c
    src/os_tasks/logger.c
    src/os_tasks/task_cli.c
    src/app/elevator_fsm.c
)

# 2. 纯净的 FreeRTOS 内核源文件
set(FREERTOS_SOURCES
    ${FREERTOS_SRC_DIR}/tasks.c
    ${FREERTOS_SRC_DIR}/queue.c
    ${FREERTOS_SRC_DIR}/list.c
    ${FREERTOS_SRC_DIR}/timers.c
    ${FREERTOS_SRC_DIR}/portable/GCC/ARM_CM3/port.c
# heap_4.c (dynamic heap) intentionally omitted to enforce static-only allocation
)

# 生成可执行文件
add_executable(RTOSDemo ${PROJECT_SOURCES} ${FREERTOS_SOURCES})

# 包含路径
target_include_directories(RTOSDemo PRIVATE
    src
    src/bsp
    src/os_tasks
    src/app
    ${FREERTOS_SRC_DIR}/include
    ${FREERTOS_SRC_DIR}/portable/GCC/ARM_CM3
)
