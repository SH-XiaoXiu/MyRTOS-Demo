//
// Created by XiaoXiu on 8/28/2025.
//

#ifndef MYRTOS_PORT_H
#define MYRTOS_PORT_H
#include <stdint.h>


/**
 * @brief 初始化平台相关的硬件。
 *        通常用于初始化调试串口等。
 */
void MyRTOS_Platform_Init(void);

/**
 * @brief 通过硬件发送一个字符。
 *        这是所有上层I/O服务的最终出口。
 * @param c 要发送的字符。
 */
void MyRTOS_Platform_PutChar(char c);

//TODO 暂没做抽象
/**
 * @brief 初始化调试输出硬件 (如: USART)。
 */
void MyRTOS_Platform_Debug_Init(void);


/**
 * @brief 初始化并启动系统Tick定时器。
 * @param tick_rate_hz 系统Tick频率。
 * @return 0 成功, -1 失败.
 */
int MyRTOS_Platform_Tick_Init(uint32_t tick_rate_hz);

/**
 * @brief 配置 PendSV 和 SysTick 的中断优先级，为调度器做准备。
 */
void MyRTOS_Platform_Scheduler_Init(void);

/**
 * @brief 初始化用户输入硬件 (如: 按键中断)。
 *        这个函数应该配置中断，并在中断处理函数中调用一个回调。
 * @param callback 当输入事件发生时要调用的函数指针。
 */
void MyRTOS_Platform_UserInput_Init(void (*callback)(void));

/**
 * @brief 初始化一个高分辨率定时器。
 */
void MyRTOS_Platform_PerfTimer_Init(void);

/**
 * @brief 获取高分辨率定时器的当前计数值。
 */
uint32_t MyRTOS_Platform_PerfTimer_GetCount(void);

#endif // MYRTOS_PORT_H