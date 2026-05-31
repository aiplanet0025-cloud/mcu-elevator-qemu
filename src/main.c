#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "bsp_uart.h"
#include "logger.h"
#include "elevator_fsm.h"
#include <stdio.h>

// 1. 定义多任务通信队列句柄
QueueHandle_t xFloorQueue = NULL;

typedef struct {
    int targetFloor;
} ElevatorEvent;

/* --- 任务一：串口输入扫描任务（低优先级） --- */
void vTaskInput(void *pvParameters) {
    char c;
    ElevatorEvent xEvent;
    
    Logger_Info("\r\n================================\r\n");
    Logger_Info("  Elevator OS Running (FreeRTOS) \r\n");
    Logger_Info("  Press '1', '2', '3' to call    \r\n");
    Logger_Info("================================\r\n");

    for (;;) {
        if (BSP_UART_GetCharNonBlocking(&c)) {
            if (c >= '1' && c <= '3') {
                xEvent.targetFloor = c - '0';
                
                // 将指令塞入消息队列
                xQueueSend(xFloorQueue, &xEvent, 0); 
                Logger_Info("\r\n[Input Task] Key pressed: calling floor %d...\r\n", xEvent.targetFloor);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 每 100ms 轮询一次输入
    }
}

/* --- 任务二：电梯主控制任务（高优先级） --- */
void vTaskElevator(void *pvParameters) {
    ElevatorFsm myElevator;
    Elevator_FsmInit(&myElevator);
    
    ElevatorEvent xReceivedEvent;
    char log_buf[128];

    for (;;) {
        // 如果电梯处于待机状态，则挂起任务，无限期阻塞等待队列事件
        if (myElevator.state == ELEVATOR_STATE_IDLE) {
            if (xQueueReceive(xFloorQueue, &xReceivedEvent, portMAX_DELAY) == pdTRUE) {
                Elevator_FsmSetTarget(&myElevator, xReceivedEvent.targetFloor);
            }
        }

        // 定时轮询时间：位移状态每层花费 1000ms；开门状态下为了精确处理 2s 的关门定时，每 500ms 迭代一次
        int tick_time_ms = 1000;
        if (myElevator.state == ELEVATOR_STATE_DOOR_OPEN) {
            tick_time_ms = 500;
        }

        // 迭代纯 C 状态机
        if (Elevator_FsmTick(&myElevator, tick_time_ms, log_buf, sizeof(log_buf))) {
            if (log_buf[0] != '\0') {
                Logger_Info(log_buf); // 输出经过互斥锁保护的安全日志
            }
        }

        vTaskDelay(pdMS_TO_TICKS(tick_time_ms));
    }
}

/* --- 系统入口 --- */
int main(void) {
    BSP_UART_Init();
    Logger_Init();

    // 1. 创建队列（深度为 10）
    xFloorQueue = xQueueCreate(10, sizeof(ElevatorEvent));

    // 2. 创建任务（分配 512 字的安全栈空间）
    xTaskCreate(vTaskInput, "Input", 512, NULL, 1, NULL);
    xTaskCreate(vTaskElevator, "Elevator", 512, NULL, 2, NULL);

    // 3. 启动 FreeRTOS 调度器
    vTaskStartScheduler();

    for (;;);
    return 0;
}

/* --- 必需的空闲与系统钩子函数（保持不变） --- */
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