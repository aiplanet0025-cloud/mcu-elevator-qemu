#include "task_cli.h"
#include "bsp_uart.h"
#include "logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "elevator_fsm.h"
#include <string.h>
#include <stdlib.h>

extern QueueHandle_t xFloorQueue;
extern ElevatorFsm g_elevator_fsm; // 声明 main.c 中的全局电梯状态机，用于状态查询

typedef struct {
    int targetFloor;
} ElevatorEvent;

// 解析并执行终端输入的指令
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

// CLI 命令行循环读取任务
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
            // 1. 处理回车换行，执行指令
            if (c == '\r' || c == '\n') {
                cmd_buf[cmd_idx] = '\0';
                if (cmd_idx > 0) {
                    parse_command(cmd_buf);
                } else {
                    Logger_Info("\r\n");
                }
                cmd_idx = 0; // 清空缓冲区
                Logger_Info("elevator> ");
            } 
            // 2. 处理退格键 (Backspace 0x08 或 Delete 0x7F)
            else if (c == 0x08 || c == 0x7F) {
                if (cmd_idx > 0) {
                    cmd_idx--;
                    // 发送经典的 VT100 终端退格转义序列，擦除屏幕上的字符
                    BSP_UART_SendString("\b \b");
                }
            } 
            // 3. 接收并显示正常字符
            else if (cmd_idx < sizeof(cmd_buf) - 1) {
                BSP_UART_SendChar(c); // 回显字符
                cmd_buf[cmd_idx++] = c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 20ms 的极高响应速率
    }
}