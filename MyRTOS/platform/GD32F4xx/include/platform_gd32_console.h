//
// Created by XiaoXiu on 8/30/2025.
//

#ifndef PLATFORM_GD32_CONSOLE_H
#define PLATFORM_GD32_CONSOLE_H

#include "MyRTOS_Stream_Def.h"


/**
 * @brief 调试时,输出一个字符到控制台。无关RTOS
 * @param c 要打印的字符
 */
void Platform_fault_putchar(char c);

/**
 * @brief 初始化GD32平台的调试控制台。
 *
 * 这个函数应该在硬件初始化之后，RTOS启动之前调用。
 * 它会配置USART0硬件。
 */
void Platform_Console_HwInit(void);

/**
 * @brief 获取一个指向已初始化的控制台流的句柄。
 *
 * 在调用 GD32_Console_HwInit() 之后，应用程序可以通过此函数获取
 * 一个实现了 Stream 接口的句柄，用于连接到RTOS的StdIO服务。
 *
 * @return StreamHandle_t 指向控制台流的句柄。
 */
StreamHandle_t Platform_Console_GetStream(void);


#endif // PLATFORM_GD32_CONSOLE_H
