//
// Created by XiaoXiu on 8/29/2025.
//

#ifndef MYRTOS_PLATFORM_H
#define MYRTOS_PLATFORM_H

/**
 * @brief 初始化平台相关的硬件，如时钟、调试串口等。
 *        此函数应在RTOS调度器启动前，在main函数中被调用。
 */
void MyRTOS_Platform_Init(void);

/**
 * @brief 通过硬件发送一个字符。
 *        这是所有上层I/O服务（如Log, Monitor）的最终出口。
 * @param c 要发送的字符。
 */
void MyRTOS_Platform_PutChar(char c);

#endif // MYRTOS_PLATFORM_H