//
// Created by XiaoXiu on 8/30/2025.
//


#ifndef PLATFORM_HIRES_TIMER_H
#define PLATFORM_HIRES_TIMER_H

#include <stdint.h>

/**
 * @brief 初始化高精度硬件定时器。
 *        配置一个硬件定时器作为自由运行的计数器。
 */
void Platform_HiresTimer_Init(void);

/**
 * @brief 获取高精度定时器的当前计数值。
 *        此函数符合 MonitorGetHiresTimerValueFn 标准。
 * @return uint32_t 32位计数值。
 */
uint32_t Platform_Timer_GetHiresValue(void);


#endif // PLATFORM_HIRES_TIMER_H