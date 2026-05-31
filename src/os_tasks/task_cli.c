#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "bsp_uart.h"
#include <string.h>
#include <stdlib.h>

extern QueueHandle_t xFloorQueue; // 电梯事件队列
extern void Logger_Info(const char *format, ...);

void vTaskCLI(void *pvParameters) {
    char cmd_buf[32];
    int cmd_idx = 0;

    for (;;) {
        char c;
        if (BSP_UART_GetCharNonBlocking(&c)) {
            // 回显用户输入的字符
            BSP_UART_SendChar(c);

            if (c == '\r' || c == '\n') {
                cmd_buf[cmd_idx] = '\0';
                if (cmd_idx > 0) {
                    UART_Print("\r\n"); // 换行
                    // 解析命令
                    if (strncmp(cmd_buf, "status", 6) == 0) {
                        Logger_Info("[CLI] Querying system status...\r\n");
                        // 触发状态查询（这里可以设计全局变量或队列查询）
                    } 
                    else if (strncmp(cmd_buf, "call ", 5) == 0) {
                        int floor = atoi(&cmd_buf[5]);
                        if (floor >= 1 && floor <= 3) {
                            ElevatorEvent event = { floor };
                            xQueueSend(xFloorQueue, &event, 0);
                            Logger_Info("[CLI] Command Accepted: Call to floor %d\r\n", floor);
                        } else {
                            Logger_Info("[CLI] Error: Invalid floor %d (Valid: 1-3)\r\n", floor);
                        }
                    } else {
                        Logger_Info("[CLI] Unknown Command: %s\r\n", cmd_buf);
                    }
                }
                cmd_idx = 0; // 重置缓冲区
            } else if (cmd_idx < sizeof(cmd_buf) - 1) {
                cmd_buf[cmd_idx++] = c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 20ms 扫描一次
    }
}