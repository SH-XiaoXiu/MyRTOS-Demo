#ifndef MYRTOS_PORT_H
#define MYRTOS_PORT_H

#include <stdint.h>

// 平台无关的数据结构
typedef uint32_t StackType_t; // CPU栈的自然宽度
typedef int32_t BaseType_t; // CPU的有符号最优宽度
typedef uint32_t UBaseType_t; // CPU的无符号最优宽度


// 移植层函数原型
/**
 * @brief 初始化任务的栈帧。
 * @param pxTopOfStack 任务栈的最高地址。
 * @param pxCode 任务函数的入口地址。
 * @param pvParameters 传递给任务的参数。
 * @return 返回初始化后任务的栈顶指针 (SP)。
 */
StackType_t *MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters);

/**
 * @brief 启动RTOS调度器。
 *        此函数负责配置硬件并启动第一个任务，且永远不会返回。
 * @return 如果成功启动则不返回，如果失败则返回非零值。
 */
BaseType_t MyRTOS_Port_StartScheduler(void);

/**
 * @brief 进入临界区。
 *        必须由平台移植层实现，以保证操作的原子性。
 */
void MyRTOS_Port_EnterCritical(void);

/**
 * @brief 退出临界区。
 */
void MyRTOS_Port_ExitCritical(void);

/**
 * @brief 在任务上下文中请求一次上下文切换。
 */
void MyRTOS_Port_Yield(void);

/**
 * @brief 在中断服务程序(ISR)上下文中请求一次上下文切换。
 * @param higherPriorityTaskWoken 如果在ISR中唤醒了一个更高优先级的任务，
 *                                 则此变量应为非零值，此时才需要触发切换。
 */
void MyRTOS_Port_YieldFromISR(BaseType_t higherPriorityTaskWoken);

#endif // MYRTOS_PORT_H
