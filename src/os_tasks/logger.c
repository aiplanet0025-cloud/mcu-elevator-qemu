#include "logger.h"
#include "bsp_uart.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdio.h>
#include <stdarg.h>

static SemaphoreHandle_t xLogMutex = NULL;
static StaticSemaphore_t xLogMutexBuffer;

void Logger_Init(void) {
    // 使用静态互斥锁保护串口输出，避免运行时动态内存分配。
    xLogMutex = xSemaphoreCreateMutexStatic(&xLogMutexBuffer);
}

void Logger_Info(const char *format, ...) {
    char buf[128];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (xLogMutex != NULL && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        // 如果调度器已经启动，使用互斥锁进行阻塞保护
        if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
            BSP_UART_SendString(buf);
            xSemaphoreGive(xLogMutex);
        }
    } else {
        // 如果调度器还未启动，直接裸机发送，避免死锁
        BSP_UART_SendString(buf);
    }
}