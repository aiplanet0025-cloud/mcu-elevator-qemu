#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "bsp_uart.h"
#include "logger.h"
#include "elevator_fsm.h"
#include "elevator_event.h"
#include "task_cli.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>

// 1. 定义多任务通信队列句柄
QueueHandle_t xFloorQueue = NULL;

// 2. 将状态机实例提升为全局变量，供 CLI 任务跨任务安全读取
ElevatorFsm g_elevator_fsm;


/* ==================================================================== */
/* 工业级安全加固：在编译期定义所有任务和队列的静态内存缓冲区             */
/* ==================================================================== */

// 1. CLI 任务的静态内存分配
#define CLI_TASK_STACK_SIZE 400
static StaticTask_t xCLITaskBuffer;
static StackType_t xCLIStack[ CLI_TASK_STACK_SIZE ];

// 2. Elevator 任务的静态内存分配
#define ELEVATOR_TASK_STACK_SIZE 400
static StaticTask_t xElevatorTaskBuffer;
static StackType_t xElevatorStack[ ELEVATOR_TASK_STACK_SIZE ];

// 3. 消息队列的静态内存分配（长度为 10）
#define QUEUE_LENGTH 10
#define ITEM_SIZE    sizeof(ElevatorEvent)
static StaticQueue_t xStaticQueue;
static uint8_t ucQueueStorageArea[ QUEUE_LENGTH * ITEM_SIZE ];


/* --- 任务：电梯主控制任务（高优先级） --- */
void vTaskElevator(void *pvParameters) {
    Elevator_FsmInit(&g_elevator_fsm); // 初始化全局状态机
    
    ElevatorEvent xReceivedEvent;
    char log_buf[128];

    for (;;) {
        // 如果电梯处于待机状态，则挂起任务，无限期阻塞等待队列事件
        if (g_elevator_fsm.state == ELEVATOR_STATE_IDLE) {
            if (xQueueReceive(xFloorQueue, &xReceivedEvent, portMAX_DELAY) == pdTRUE) {
                Elevator_FsmSetTarget(&g_elevator_fsm, xReceivedEvent.targetFloor);
            }
        }

        int tick_time_ms = 1000;
        if (g_elevator_fsm.state == ELEVATOR_STATE_DOOR_OPEN) {
            tick_time_ms = 500;
        }

        // 迭代状态机
        if (Elevator_FsmTick(&g_elevator_fsm, tick_time_ms, log_buf, sizeof(log_buf))) {
            if (log_buf[0] != '\0') {
                Logger_Info(log_buf); 
            }
        }

        vTaskDelay(pdMS_TO_TICKS(tick_time_ms));
    }
}

/* --- 系统入口 --- */
int main(void) {
    BSP_UART_Init();
    Logger_Init();

    // 1. 工业安全加固：使用静态 API 创建队列，杜绝运行时内存碎片
    xFloorQueue = xQueueCreateStatic(
        QUEUE_LENGTH, 
        ITEM_SIZE, 
        ucQueueStorageArea, 
        &xStaticQueue
    );

    // 2. 工业安全加固：使用静态 API 创建系统应用任务
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

    // 3. 启动 FreeRTOS 调度器
    vTaskStartScheduler();

    for (;;);
    return 0;
}

/* --- 必需的空闲与系统静态内存分配钩子函数（保持不变） --- */
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

/* --- 补充的中断与底层存根（保持不变） --- */
void Timer0IntHandler(void) {}
void vT2InterruptHandler(void) {}
void vT3InterruptHandler(void) {}

void * _sbrk(ptrdiff_t incr) {
    (void)incr;
    errno = ENOMEM;
    return (void *)-1;
}
