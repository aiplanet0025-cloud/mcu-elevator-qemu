cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Please configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake so CMake detects arm-none-eabi-gcc before project().")
endif()

project(RTOSDemo C ASM)

# 路径定义
set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/FreeRTOS)
set(FREERTOS_SRC_DIR ${FREERTOS_DIR}/Source)

# 1. 我们的项目源文件（加入了 elevator_fsm.c）
set(PROJECT_SOURCES
    src/main.c
    src/bsp/bsp_uart.c
    src/bsp/startup.c
    src/os_tasks/logger.c
    src/os_tasks/task_cli.c
    src/app/elevator_fsm.c                         # <--- 新加入的文件
)

# 2. 纯净的 FreeRTOS 内核源文件
set(FREERTOS_SOURCES
    ${FREERTOS_SRC_DIR}/tasks.c
    ${FREERTOS_SRC_DIR}/queue.c
    ${FREERTOS_SRC_DIR}/list.c
    ${FREERTOS_SRC_DIR}/timers.c
    ${FREERTOS_SRC_DIR}/portable/GCC/ARM_CM3/port.c
    ${FREERTOS_SRC_DIR}/portable/MemMang/heap_4.c
)

# 生成可执行文件
add_executable(RTOSDemo ${PROJECT_SOURCES} ${FREERTOS_SOURCES})

# 包含路径（加入了 src/app）
target_include_directories(RTOSDemo PRIVATE
    src
    src/bsp
    src/os_tasks
    src/app                                        # <--- 新加入的头文件搜索路径
    ${FREERTOS_SRC_DIR}/include
    ${FREERTOS_SRC_DIR}/portable/GCC/ARM_CM3
)