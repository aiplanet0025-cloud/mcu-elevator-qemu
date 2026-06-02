#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "bsp_uart.h"
#include "logger.h"
#include "elevator_fsm.h"
#include "elevator_event.h"
#include "dispatcher_event.h"
#include "task_cli.h"
#include "task_elevator.h"
#include "task_dispatcher.h"
#include <stdio.h>

// 1. 定义多任务通信队列句柄
QueueHandle_t xFloorQueue = NULL;
QueueHandle_t xDispatcherQueue = NULL;

// 2. 将状态机实例提升为全局变量，供 CLI 任务跨任务安全读取
ElevatorFsm g_elevator_fsm1;
ElevatorFsm g_elevator_fsm2;


/* ==================================================================== */
/* 工业级安全加固：在编译期定义所有任务和队列的静态内存缓冲区             */
/* ==================================================================== */

// 1. CLI 任务的静态内存分配
#define CLI_TASK_STACK_SIZE 400
static StaticTask_t xCLITaskBuffer;
static StackType_t xCLIStack[ CLI_TASK_STACK_SIZE ];

// 2. Elevator 任务的静态内存分配
#define ELEVATOR_TASK_STACK_SIZE 400
static StaticTask_t xElevator1TaskBuffer;
static StackType_t xElevator1Stack[ ELEVATOR_TASK_STACK_SIZE ];
static StaticTask_t xElevator2TaskBuffer;
static StackType_t xElevator2Stack[ ELEVATOR_TASK_STACK_SIZE ];

// 3. Dispatcher 任务的静态内存分配
#define DISPATCHER_TASK_STACK_SIZE 400
static StaticTask_t xDispatcherTaskBuffer;
static StackType_t xDispatcherStack[ DISPATCHER_TASK_STACK_SIZE ];

// 4. 消息队列的静态内存分配
#define QUEUE_LENGTH 10
#define ITEM_SIZE    sizeof(ElevatorEvent)
static StaticQueue_t xStaticQueue;
static uint8_t ucQueueStorageArea[ QUEUE_LENGTH * ITEM_SIZE ];

static StaticQueue_t xStaticDispatcherQueue;
static uint8_t ucDispatcherQueueStorageArea[ QUEUE_LENGTH * sizeof(DispatcherEvent) ];


/* --- 系统入口 --- */
int main(void) {
    BSP_UART_Init();
    Logger_Init();

    // 工业安全加固：使用静态 API 创建队列
    xFloorQueue = xQueueCreateStatic(
        QUEUE_LENGTH, 
        ITEM_SIZE, 
        ucQueueStorageArea, 
        &xStaticQueue
    );

    xDispatcherQueue = xQueueCreateStatic(
        QUEUE_LENGTH, 
        sizeof(DispatcherEvent), 
        ucDispatcherQueueStorageArea, 
        &xStaticDispatcherQueue
    );

    // CLI 任务
    xTaskCreateStatic(
        vTaskCLI, 
        "CLI", 
        CLI_TASK_STACK_SIZE, 
        NULL, 
        1, 
        xCLIStack, 
        &xCLITaskBuffer
    ); 

    // Dispatcher 任务
    xTaskCreateStatic(
        vTaskDispatcher, 
        "Dispatcher", 
        DISPATCHER_TASK_STACK_SIZE, 
        NULL, 
        3, // High priority
        xDispatcherStack, 
        &xDispatcherTaskBuffer
    );

    // 电梯任务
    g_elevator_fsm1.elevator_id = 1;
    xTaskCreateStatic(
        vTaskElevator, 
        "Elevator1", 
        ELEVATOR_TASK_STACK_SIZE, 
        &g_elevator_fsm1, 
        2, 
        xElevator1Stack, 
        &xElevator1TaskBuffer
    );

    g_elevator_fsm2.elevator_id = 2;
    xTaskCreateStatic(
        vTaskElevator, 
        "Elevator2", 
        ELEVATOR_TASK_STACK_SIZE, 
        &g_elevator_fsm2, 
        2, 
        xElevator2Stack, 
        &xElevator2TaskBuffer
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
