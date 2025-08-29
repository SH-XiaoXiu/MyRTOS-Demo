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


/**
 * @brief 从硬件获取一个字符（非阻塞）。
 *        此函数立即返回。
 * @param c 指向用于存储接收字符的变量的指针。
 * @return 1 表示成功获取一个字符, 0 表示当前没有可用字符。
 */
int MyRTOS_Platform_GetChar(char *c);

/**
 * @brief 从硬件获取一个字符（阻塞式）。
 *        此函数将阻塞任务，直到接收到一个新字符。
 *        这是输入驱动型任务（如Terminal）的理想选择。
 * @param c 指向用于存储接收字符的变量的指针。
 * @return 1 表示成功获取一个字符, 0 表示超时或失败 (在此实现中永不为0)。
 */
int MyRTOS_Platform_GetChar_Blocking(char *c);


#endif // MYRTOS_PLATFORM_H