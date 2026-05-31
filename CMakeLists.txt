cmake_minimum_required(VERSION 3.13)
project(RTOSDemo C ASM)

# 强制使用静态库编译测试，绕过裸机编译检测
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 指定交叉编译器参数（带有扩充主栈的 -DSTACK_SIZE=2048 参数）
set(CMAKE_C_FLAGS "-mcpu=cortex-m3 -mthumb -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -DSTACK_SIZE=2048")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m3 -mthumb")

# 指定编译器
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# 链接器参数
set(CMAKE_EXE_LINKER_FLAGS "-mcpu=cortex-m3 -mthumb -T ${CMAKE_CURRENT_SOURCE_DIR}/standalone.ld --specs=nosys.specs --specs=nano.specs -nostartfiles")

# 路径定义
set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/FreeRTOS)
set(FREERTOS_SRC_DIR ${FREERTOS_DIR}/Source)

# 1. 我们的项目源文件（加入了 elevator_fsm.c）
set(PROJECT_SOURCES
    src/main.c
    src/bsp/bsp_uart.c
    src/bsp/startup.c
    src/os_tasks/logger.c
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