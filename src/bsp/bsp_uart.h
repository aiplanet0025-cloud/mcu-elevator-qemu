#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>

void BSP_UART_Init(void);
void BSP_UART_SendChar(char c);
void BSP_UART_SendString(const char *str);
bool BSP_UART_GetCharNonBlocking(char *out_char);

#endif