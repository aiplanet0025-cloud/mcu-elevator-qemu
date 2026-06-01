#!/bin/bash
set -e

echo "=== 1. 检查并准备 FreeRTOS 依赖库 ==="
if [ ! -d "FreeRTOS" ]; then
    echo "未检测到 FreeRTOS 文件夹，正在克隆官方仓库..."
    git clone --depth 1 https://github.com/FreeRTOS/FreeRTOS.git
fi

echo "正在拉取 FreeRTOS 核心子模块..."
cd FreeRTOS
git submodule update --init --recursive FreeRTOS/Source
cd ..

echo "=== 2. 创建标准工业级嵌入式工程结构 ==="
mkdir -p src/bsp src/os_tasks src/app cmake build

echo "=== 3. 复制芯片配置文件和启动文件 ==="
cp FreeRTOS/FreeRTOS/Demo/CORTEX_LM3S6965_GCC_QEMU/standalone.ld ./
cp FreeRTOS/FreeRTOS/Demo/CORTEX_LM3S6965_GCC_QEMU/FreeRTOSConfig.h ./src/
cp FreeRTOS/FreeRTOS/Demo/CORTEX_LM3S6965_GCC_QEMU/startup.c ./src/bsp/

echo "=== 4. 写入底盘驱动 src/bsp/bsp_uart.h ==="
cat << 'INNER_EOF' > src/bsp/bsp_uart.h
#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>

void BSP_UART_Init(void);
void BSP_UART_SendChar(char c);
void BSP_UART_SendString(const char *str);
bool BSP_UART_GetCharNonBlocking(char *out_char);

#endif
INNER_EOF

echo "=== 5. 写入底盘驱动 src/bsp/bsp_uart.c ==="
cat << 'INNER_EOF' > src/bsp/bsp_uart.c
#include "bsp_uart.h"

#define UART0_DR (*((volatile uint32_t *)0x4000C000))
#define UART0_FR (*((volatile uint32_t *)0x4000C018))

void BSP_UART_Init(void) {
    // QEMU 模拟环境无需复杂的时钟初始化
}

void BSP_UART_SendChar(char c) {
    while (UART0_FR & (1 << 5)); 
    UART0_DR = c;
}

void BSP_UART_SendString(const char *str) {
    while (*str) {
        BSP_UART_SendChar(*str++);
    }
}

bool BSP_UART_GetCharNonBlocking(char *out_char) {
    if ((UART0_FR & (1 << 4)) == 0) { 
        *out_char = (char)(UART0_DR & 0xFF);
        return true;
    }
    return false;
}
INNER_EOF

echo "=== 6. 写入线程安全日志 src/os_tasks/logger.h ==="
cat << 'INNER_EOF' > src/os_tasks/logger.h
#ifndef LOGGER_H
#define LOGGER_H

void Logger_Init(void);
void Logger_Info(const char *format, ...);

#endif
INNER_EOF

echo "=== 7. 写入线程安全日志 src/os_tasks/logger.c ==="
cat << 'INNER_EOF' > src/os_tasks/logger.c
#include "logger.h"
#include "bsp_uart.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdio.h>
#include <stdarg.h>

static SemaphoreHandle_t xLogMutex = NULL;

void Logger_Init(void) {
    xLogMutex = xSemaphoreCreateMutex();
}

void Logger_Info(const char *format, ...) {
    char buf[128];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (xLogMutex != NULL && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
            BSP_UART_SendString(buf);
            xSemaphoreGive(xLogMutex);
        }
    } else {
        BSP_UART_SendString(buf);
    }
}
INNER_EOF

echo "=== 8. 写入诊断终端命令行 src/os_tasks/task_cli.h ==="
cat << 'INNER_EOF' > src/os_tasks/task_cli.h
#ifndef TASK_CLI_H
#define TASK_CLI_H

void vTaskCLI(void *pvParameters);

#endif
INNER_EOF

echo "=== 8a. 写入统一事件类型头文件 src/app/elevator_event.h ==="
cat << 'INNER_EOF' > src/app/elevator_event.h
#ifndef ELEVATOR_EVENT_H
#define ELEVATOR_EVENT_H

typedef struct {
    int targetFloor;
} ElevatorEvent;

#endif
INNER_EOF

echo "=== 9. 写入诊断终端命令行 src/os_tasks/task_cli.c ==="
cat << 'INNER_EOF' > src/os_tasks/task_cli.c
#include "task_cli.h"
#include "bsp_uart.h"
#include "logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "elevator_fsm.h"
#include "elevator_event.h"
#include <string.h>
#include <stdlib.h>

extern QueueHandle_t xFloorQueue;
extern ElevatorFsm g_elevator_fsm;


static void parse_command(char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        Logger_Info("\r\n--- Diagnostics Commands ---\r\n");
        Logger_Info("  help       - Show this help menu\r\n");
        Logger_Info("  status     - Show elevator current metrics\r\n");
        Logger_Info("  call <1-3> - Call elevator to a specific floor\r\n");
    } 
    else if (strcmp(cmd, "status") == 0) {
        const char *state_str = "UNKNOWN";
        switch (g_elevator_fsm.state) {
            case ELEVATOR_STATE_IDLE:          state_str = "IDLE"; break;
            case ELEVATOR_STATE_MOVING_UP:     state_str = "MOVING UP"; break;
            case ELEVATOR_STATE_MOVING_DOWN:   state_str = "MOVING DOWN"; break;
            case ELEVATOR_STATE_DOOR_OPENING:  state_str = "DOOR OPENING"; break;
            case ELEVATOR_STATE_DOOR_OPEN:     state_str = "DOOR OPEN"; break;
            case ELEVATOR_STATE_DOOR_CLOSING:  state_str = "DOOR CLOSING"; break;
        }
        Logger_Info("\r\n[Status] Curr Floor: %d | Target Floor: %d | Motor State: %s\r\n", 
                    g_elevator_fsm.current_floor, g_elevator_fsm.target_floor, state_str);
    } 
    else if (strncmp(cmd, "call ", 5) == 0) {
        int floor = atoi(&cmd[5]);
        if (floor >= 1 && floor <= 3) {
            ElevatorEvent xEvent = { floor };
            xQueueSend(xFloorQueue, &xEvent, 0);
            Logger_Info("\r\n[CLI] Success: Event 'call %d' queued.\r\n", floor);
        } else {
            Logger_Info("\r\n[CLI] Error: Invalid floor %d (Valid: 1 to 3).\r\n", floor);
        }
    } 
    else {
        Logger_Info("\r\n[CLI] Error: Unknown command '%s'. Type 'help' for support.\r\n", cmd);
    }
}

void vTaskCLI(void *pvParameters) {
    static char cmd_buf[64];
    static int cmd_idx = 0;
    char c;

    Logger_Info("\r\n==============================================\r\n");
    Logger_Info("  Elevator Diagnostics Console (CLI) Ready\r\n");
    Logger_Info("  Type 'help' to see available commands.\r\n");
    Logger_Info("==============================================\r\n");
    Logger_Info("elevator> ");

    for (;;) {
        if (BSP_UART_GetCharNonBlocking(&c)) {
            if (c == '\r' || c == '\n') {
                cmd_buf[cmd_idx] = '\0';
                if (cmd_idx > 0) {
                    parse_command(cmd_buf);
                } else {
                    Logger_Info("\r\n");
                }
                cmd_idx = 0; 
                Logger_Info("elevator> ");
            } 
            else if (c == 0x08 || c == 0x7F) {
                if (cmd_idx > 0) {
                    cmd_idx--;
                    BSP_UART_SendString("\b \b");
                }
            } 
            else if (cmd_idx < sizeof(cmd_buf) - 1) {
                BSP_UART_SendChar(c); 
                cmd_buf[cmd_idx++] = c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}
INNER_EOF

echo "=== 10. 写入纯 C 电梯状态机 src/app/elevator_fsm.h ==="
cat << 'INNER_EOF' > src/app/elevator_fsm.h
#ifndef ELEVATOR_FSM_H
#define ELEVATOR_FSM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ELEVATOR_STATE_IDLE,
    ELEVATOR_STATE_MOVING_UP,
    ELEVATOR_STATE_MOVING_DOWN,
    ELEVATOR_STATE_DOOR_OPENING,
    ELEVATOR_STATE_DOOR_OPEN,
    ELEVATOR_STATE_DOOR_CLOSING
} FsmState;

typedef struct {
    int current_floor;
    int target_floor;
    FsmState state;
    uint32_t timer_ms; 
} ElevatorFsm;

void Elevator_FsmInit(ElevatorFsm *fsm);
void Elevator_FsmSetTarget(ElevatorFsm *fsm, int target);
bool Elevator_FsmTick(ElevatorFsm *fsm, int tick_ms, char *log_output, int log_size);

#endif
INNER_EOF

echo "=== 11. 写入纯 C 电梯状态机 src/app/elevator_fsm.c ==="
cat << 'INNER_EOF' > src/app/elevator_fsm.c
#include "elevator_fsm.h"
#include <stdio.h>

void Elevator_FsmInit(ElevatorFsm *fsm) {
    fsm->current_floor = 1;
    fsm->target_floor = 1;
    fsm->state = ELEVATOR_STATE_IDLE;
    fsm->timer_ms = 0;
}

void Elevator_FsmSetTarget(ElevatorFsm *fsm, int target) {
    if (fsm->state == ELEVATOR_STATE_IDLE) {
        fsm->target_floor = target;
    }
}

bool Elevator_FsmTick(ElevatorFsm *fsm, int tick_ms, char *log_output, int log_size) {
    log_output[0] = '\0';
    bool state_changed = false;

    switch (fsm->state) {
        case ELEVATOR_STATE_IDLE:
            if (fsm->current_floor != fsm->target_floor) {
                if (fsm->target_floor > fsm->current_floor) {
                    fsm->state = ELEVATOR_STATE_MOVING_UP;
                    snprintf(log_output, log_size, "[FSM] Call Accepted! Target set to %d. Motor: STARTING UP...\r\n", fsm->target_floor);
                } else {
                    fsm->state = ELEVATOR_STATE_MOVING_DOWN;
                    snprintf(log_output, log_size, "[FSM] Call Accepted! Target set to %d. Motor: STARTING DOWN...\r\n", fsm->target_floor);
                }
                state_changed = true;
            }
            break;

        case ELEVATOR_STATE_MOVING_UP:
            fsm->current_floor++;
            snprintf(log_output, log_size, "[FSM] >> Elevator arrived at floor %d.\r\n", fsm->current_floor);
            state_changed = true;
            
            if (fsm->current_floor == fsm->target_floor) {
                fsm->state = ELEVATOR_STATE_DOOR_OPENING;
            }
            break;

        case ELEVATOR_STATE_MOVING_DOWN:
            fsm->current_floor--;
            snprintf(log_output, log_size, "[FSM] >> Elevator arrived at floor %d.\r\n", fsm->current_floor);
            state_changed = true;
            
            if (fsm->current_floor == fsm->target_floor) {
                fsm->state = ELEVATOR_STATE_DOOR_OPENING;
            }
            break;

        case ELEVATOR_STATE_DOOR_OPENING:
            snprintf(log_output, log_size, "[FSM] Door: OPENING at floor %d.\r\n", fsm->current_floor);
            state_changed = true;
            fsm->state = ELEVATOR_STATE_DOOR_OPEN;
            fsm->timer_ms = 0;
            break;

        case ELEVATOR_STATE_DOOR_OPEN:
            fsm->timer_ms += tick_ms;
            if (fsm->timer_ms >= 2000) { 
                fsm->state = ELEVATOR_STATE_DOOR_CLOSING;
                snprintf(log_output, log_size, "[FSM] Door: CLOSING...\r\n");
                state_changed = true;
            }
            break;

        case ELEVATOR_STATE_DOOR_CLOSING:
            snprintf(log_output, log_size, "[FSM] Door: CLOSED. Elevator is now IDLE.\r\n");
            state_changed = true;
            fsm->state = ELEVATOR_STATE_IDLE;
            break;
    }
    return state_changed;
}
INNER_EOF

echo "=== 12. 写入系统入口与静态安全加固配置 src/main.c ==="
cat << 'INNER_EOF' > src/main.c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "bsp_uart.h"
#include "logger.h"
#include "elevator_fsm.h"
#include "task_cli.h"
#include "elevator_event.h"
#include <stdio.h>

QueueHandle_t xFloorQueue = NULL;
ElevatorFsm g_elevator_fsm;


/* ==================================================================== */
/* 工业级安全加固：在编译期定义所有任务和队列的静态内存缓冲区             */
/* ==================================================================== */
#define CLI_TASK_STACK_SIZE 400
static StaticTask_t xCLITaskBuffer;
static StackType_t xCLIStack[ CLI_TASK_STACK_SIZE ];

#define ELEVATOR_TASK_STACK_SIZE 400
static StaticTask_t xElevatorTaskBuffer;
static StackType_t xElevatorStack[ ELEVATOR_TASK_STACK_SIZE ];

#define QUEUE_LENGTH 10
#define ITEM_SIZE    sizeof(ElevatorEvent)
static StaticQueue_t xStaticQueue;
static uint8_t ucQueueStorageArea[ QUEUE_LENGTH * ITEM_SIZE ];

void vTaskElevator(void *pvParameters) {
    Elevator_FsmInit(&g_elevator_fsm); 
    ElevatorEvent xReceivedEvent;
    char log_buf[128];

    for (;;) {
        if (g_elevator_fsm.state == ELEVATOR_STATE_IDLE) {
            if (xQueueReceive(xFloorQueue, &xReceivedEvent, portMAX_DELAY) == pdTRUE) {
                Elevator_FsmSetTarget(&g_elevator_fsm, xReceivedEvent.targetFloor);
            }
        }

        int tick_time_ms = 1000;
        if (g_elevator_fsm.state == ELEVATOR_STATE_DOOR_OPEN) {
            tick_time_ms = 500;
        }

        if (Elevator_FsmTick(&g_elevator_fsm, tick_time_ms, log_buf, sizeof(log_buf))) {
            if (log_buf[0] != '\0') {
                Logger_Info(log_buf); 
            }
        }

        vTaskDelay(pdMS_TO_TICKS(tick_time_ms));
    }
}

int main(void) {
    BSP_UART_Init();
    Logger_Init();

    xFloorQueue = xQueueCreateStatic(
        QUEUE_LENGTH, 
        ITEM_SIZE, 
        ucQueueStorageArea, 
        &xStaticQueue
    );

    xTaskCreateStatic(
        vTaskCLI, 
        "CLI", 
        CLI_TASK_STACK_SIZE, 
        NULL, 
        1, 
        xCLIStack, 
        &xCLITaskBuffer
    ); 

    xTaskCreateStatic(
        vTaskElevator, 
        "Elevator", 
        ELEVATOR_TASK_STACK_SIZE, 
        NULL, 
        2, 
        xElevatorStack, 
        &xElevatorTaskBuffer
    );

    vTaskStartScheduler();
    for (;;);
    return 0;
}

void vApplicationMallocFailedHook(void) { for (;;); }
void vApplicationIdleHook(void) {}
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) { (void)pxTask; (void)pcTaskName; for (;;); }
void vApplicationTickHook(void) {}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vAssertCalled(const char *pcFile, uint32_t ulLine) {
    Logger_Info("\r\n[ASSERT] %s:%d\r\n", pcFile, ulLine);
    for (;;);
}

void Timer0IntHandler(void) {}
void vT2InterruptHandler(void) {}
void vT3InterruptHandler(void) {}

#include <stddef.h>
void * _sbrk(ptrdiff_t incr) {
    extern int _ebss; 
    static unsigned char *heap_end = NULL;
    unsigned char *prev_heap_end;

    if (heap_end == NULL) {
        heap_end = (unsigned char *)&_ebss;
    }
    prev_heap_end = heap_end;
    heap_end += incr;
    return (void *)prev_heap_end;
}
INNER_EOF

echo "=== 13. 写入 ARM 交叉编译工具链配置 cmake/toolchain-arm-none-eabi.cmake ==="
cat << 'INNER_EOF' > cmake/toolchain-arm-none-eabi.cmake
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
INNER_EOF

echo "=== 14. 写入顶层工程配置文件 CMakeLists.txt ==="
cat << 'INNER_EOF' > CMakeLists.txt
cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Please configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake so CMake detects arm-none-eabi-gcc before project().")
endif()

project(RTOSDemo C ASM)

set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/FreeRTOS)
set(FREERTOS_SRC_DIR ${FREERTOS_DIR}/Source)

set(PROJECT_SOURCES
    src/main.c
    src/bsp/bsp_uart.c
    src/bsp/startup.c
    src/os_tasks/logger.c
    src/os_tasks/task_cli.c
    src/app/elevator_fsm.c
)

set(FREERTOS_SOURCES
    ${FREERTOS_SRC_DIR}/tasks.c
    ${FREERTOS_SRC_DIR}/queue.c
    ${FREERTOS_SRC_DIR}/list.c
    ${FREERTOS_SRC_DIR}/timers.c
    ${FREERTOS_SRC_DIR}/portable/GCC/ARM_CM3/port.c
    ${FREERTOS_SRC_DIR}/portable/MemMang/heap_4.c
)

add_executable(RTOSDemo ${PROJECT_SOURCES} ${FREERTOS_SOURCES})

target_include_directories(RTOSDemo PRIVATE
    src
    src/bsp
    src/os_tasks
    src/app
    ${FREERTOS_SRC_DIR}/include
    ${FREERTOS_SRC_DIR}/portable/GCC/ARM_CM3
)
INNER_EOF

echo "=== 15. 链接脚本尾部异常帧规避修复 ==="
# 防止重复追加
if ! grep -q "DISCARD" standalone.ld; then
    echo 'SECTIONS { /DISCARD/ : { *(.eh_frame*) *(.eh_frame_hdr*) } }' >> standalone.ld
fi

echo "=== 🎉 一键环境初始化完成！ ==="
echo "请执行以下命令进行编译和运行："
echo "  cd build"
echo "  cmake -S .. -B . -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake"
echo "  make"
echo "  qemu-system-arm -M lm3s6965evb -kernel RTOSDemo -nographic"
