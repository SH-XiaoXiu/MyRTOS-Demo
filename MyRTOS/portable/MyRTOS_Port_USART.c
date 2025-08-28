//
// Created by XiaoXiu on 8/28/2025.
//

#include "MyRTOS_Port.h"
#include "gd32f4xx.h"
#include <stdio.h>

#include "gd32f4xx_gpio.h"
#include "lib_usart0.h"
#include "gd32f4xx_usart.h"

// 默认使用 USART0
#define MYRTOS_PORT_USARTx      USART0
#define MYRTOS_PORT_RCU_GPIO    RCU_GPIOA
#define MYRTOS_PORT_RCU_USART   RCU_USART0
#define MYRTOS_PORT_GPIO_PORT   GPIOA
#define MYRTOS_PORT_GPIO_AF     GPIO_AF_7
#define MYRTOS_PORT_TX_PIN      GPIO_PIN_9
#define MYRTOS_PORT_RX_PIN      GPIO_PIN_10

/**
 * @brief 初始化调试串口 (USART0)
 */
void MyRTOS_Platform_Init(void) {
    lib_usart0_init();
}

/**
 * @brief 通过串口发送一个字符
 */
void MyRTOS_Platform_PutChar(char c) {
    usart_data_transmit(MYRTOS_PORT_USARTx, (uint8_t)c);
    while (RESET == usart_flag_get(MYRTOS_PORT_USARTx, USART_FLAG_TC));
}