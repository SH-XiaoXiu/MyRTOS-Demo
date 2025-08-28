//
// Created by XiaoXiu on 8/29/2025.
//
// Platform: GD32F4xx Series
//

#include "gd32f4xx_usart.h"
#include "MyRTOS.h"
#include "MyRTOS_Platform.h"
#include "lib_usart0.h"

// 默认使用 USART0
#define MYRTOS_PORT_USARTx      USART0

/**
 * @brief 初始化平台相关的硬件，如时钟、调试串口等。
 */
void MyRTOS_Platform_Init(void) {
    lib_usart0_init();
}

/**
 * @brief 通过硬件发送一个字符。
 */
void MyRTOS_Platform_PutChar(char c) {
    usart_data_transmit(MYRTOS_PORT_USARTx, (uint8_t)c);
    while (RESET == usart_flag_get(MYRTOS_PORT_USARTx, USART_FLAG_TC));
}