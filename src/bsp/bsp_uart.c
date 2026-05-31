#include "bsp_uart.h"

// LM3S6965 芯片的 UART0 寄存器物理地址
#define UART0_DR (*((volatile uint32_t *)0x4000C000))
#define UART0_FR (*((volatile uint32_t *)0x4000C018))

void BSP_UART_Init(void) {
    // QEMU 仿真环境无需复杂的时钟初始化
}

void BSP_UART_SendChar(char c) {
    while (UART0_FR & (1 << 5)); // 等待发送缓冲区非空
    UART0_DR = c;
}

void BSP_UART_SendString(const char *str) {
    while (*str) {
        BSP_UART_SendChar(*str++);
    }
}

bool BSP_UART_GetCharNonBlocking(char *out_char) {
    if ((UART0_FR & (1 << 4)) == 0) { // RXFE == 0 代表接收缓冲区有数据
        *out_char = (char)(UART0_DR & 0xFF);
        return true;
    }
    return false;
}