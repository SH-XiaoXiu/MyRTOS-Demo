//
// Created by XiaoXiu on 8/28/2025.
//

#ifndef MYRTOS_PORT_H
#define MYRTOS_PORT_H

#include <stdint.h>
#include "MyRTOS_Config.h"

// =======================================================================
// CPU 架构相关的类型和宏
// =======================================================================

typedef uint32_t    StackType_t;
typedef int32_t     BaseType_t;
typedef uint32_t    UBaseType_t;



// =======================================================================
// 内核启动与栈初始化 (CPU核心移植)
// =======================================================================

/**
 * @brief 初始化任务的栈帧
 * @param pxTopOfStack 任务栈的最高地址
 * @param pxCode 任务函数的入口地址
 * @param pvParameters 传递给任务的参数
 * @return 返回初始化后任务的栈顶指针 (SP)
 */
StackType_t* MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters);

/**
 * @brief 启动RTOS调度器
 *        此函数负责配置系统定时器 (SysTick)、设置中断优先级，并启动第一个任务。
 *        此函数永远不会返回。
 * @return 如果成功启动则不返回，如果失败则返回非零值。
 */
BaseType_t MyRTOS_Port_StartScheduler(void);

#endif // MYRTOS_PORT_H