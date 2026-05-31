#include "FreeRTOS.h"
#include "task.h"
#include "bsp_uart.h"
#include "logger.h"

// 模拟任务 A：高频打印
void vTaskSenderA(void *pvParameters) {
    // 此时已经运行在 FreeRTOS 的 PSP（进程栈）上，栈空间非常宽裕（512字/2KB）
    Logger_Info("\r\n================================\r\n");
    Logger_Info("  System Booting Up (via RTOS)  \r\n");
    Logger_Info("================================\r\n");

    for (;;) {
        Logger_Info("[Task A] ---------------------------------- Hello from Sender A!\r\n");
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 模拟任务 B：高频打印
void vTaskSenderB(void *pvParameters) {
    for (;;) {
        Logger_Info("[Task B] ********************************** Hello from Sender B!\r\n");
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

int main(void) {
    // 1. 初始化底层硬件驱动
    BSP_UART_Init();

    // 2. 初始化 OS 层的线程安全日志
    Logger_Init();

    // 💥 安全避坑：不要在此处调用 Logger_Info。因为主栈（MSP）在启动期非常敏感。

    // 3. 创建两个高度竞争串口的任务 (512字/2KB 栈)
    xTaskCreate(vTaskSenderA, "SenderA", 512, NULL, 1, NULL);
    xTaskCreate(vTaskSenderB, "SenderB", 512, NULL, 1, NULL);

    // 4. 启动 FreeRTOS 调度器
    vTaskStartScheduler();

    for (;;);
    return 0;
}

/* --- 必需的空闲与系统钩子函数（保持不变） --- */
void vApplicationMallocFailedHook(void) { for (;;); }
void vApplicationIdleHook(void) {}
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) { (void)pxTask; (void)pcTaskName; for (;;); }
void vApplicationTickHook(void) {}
// 为系统 IDLE（空闲）任务提供真实的静态内存块
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, 
                                    StackType_t **ppxIdleTaskStackBuffer, 
                                    uint32_t *pulIdleTaskStackSize) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];
    
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
// 为系统 Timer（定时器）任务提供真实的静态内存块
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, 
                                     StackType_t **ppxTimerTaskStackBuffer, 
                                     uint32_t *pulTimerTaskStackSize) {
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