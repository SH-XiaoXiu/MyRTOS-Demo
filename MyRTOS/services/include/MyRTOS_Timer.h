/**
 * @brief MyRTOS 软件定时器服务 - 公共接口
 * @details 提供基于后台任务的、高精度的软件定时器功能。
 */

#ifndef MYRTOS_EXT_TIMER_H
#define MYRTOS_EXT_TIMER_H

#include "MyRTOS_Service_Config.h"


#ifndef MYRTOS_SERVICE_TIMER_ENABLE
#define MYRTOS_SERVICE_TIMER_ENABLE 0
#endif


#if MYRTOS_SERVICE_TIMER_ENABLE == 1

#include <stdint.h>

// 前置声明定时器结构体，对外部不透明，实现信息隐藏
struct Timer_t;

/** @brief 软件定时器句柄类型。*/
typedef struct Timer_t *TimerHandle_t;

/**
 * @brief 定时器回调函数类型
 * @param timer 触发回调的定时器句柄
 */
typedef void (*TimerCallback_t)(TimerHandle_t timer);

/**
 * @brief 初始化软件定时器服务。
 * @details 必须在使用任何其他定时器API之前调用此函数。
 *          它会创建一个专用的“定时器服务任务”来管理所有定时器。
 * @param timer_task_priority 定时器服务任务的优先级。通常应设为较高优先级，
 *                            以确保定时器回调的及时性。
 * @param timer_task_stack_size 定时器服务任务的栈大小。
 * @return int 0 成功, -1 失败。
 */
int TimerService_Init(uint8_t timer_task_priority, uint16_t timer_task_stack_size);

/**
 * @brief 创建一个软件定时器。
 * @param name          [in] 定时器的名称（用于调试）。
 * @param period        [in] 定时器的周期（单位：ticks）。对于单次定时器，这是触发延时。
 * @param is_periodic   [in] 1 表示为周期性定时器, 0 表示为单次定时器。
 * @param callback      [in] 定时器到期时要执行的回调函数。
 * @param p_timer_arg   [in] 传递给回调函数的自定义参数。
 * @return TimerHandle_t 成功则返回定时器句柄，失败则返回 NULL。
 */
TimerHandle_t Timer_Create(const char *name, uint32_t period, uint8_t is_periodic, TimerCallback_t callback,
                           void *p_timer_arg);

/**
 * @brief 启动一个软件定时器。
 * @details 定时器将在从此刻起 `period` ticks 后首次触发。
 * @param timer       [in] 要启动的定时器句柄。
 * @param block_ticks [in] 发送命令到定时器任务时的阻塞等待时间。0表示不等待。
 * @return int 0 命令发送成功, -1 失败。
 */
int Timer_Start(TimerHandle_t timer, uint32_t block_ticks);

/**
 * @brief 停止一个软件定时器。
 * @param timer       [in] 要停止的定时器句柄。
 * @param block_ticks [in] 发送命令到定时器任务时的阻塞等待时间。0表示不等待。
 * @return int 0 命令发送成功, -1 失败。
 */
int Timer_Stop(TimerHandle_t timer, uint32_t block_ticks);

/**
 * @brief 删除一个软件定时器。
 * @param timer       [in] 要删除的定时器句柄。
 * @param block_ticks [in] 发送命令到定时器任务时的阻塞等待时间。0表示不等待。
 * @return int 0 命令发送成功, -1 失败。
 */
int Timer_Delete(TimerHandle_t timer, uint32_t block_ticks);

/**
 * @brief 更改定时器的周期。
 * @param timer       [in] 要操作的定时器句柄。
 * @param new_period  [in] 新的周期（单位：ticks）。
 * @param block_ticks [in] 发送命令到定时器任务时的阻塞等待时间。0表示不等待。
 * @return int 0 命令发送成功, -1 失败。
 */
int Timer_ChangePeriod(TimerHandle_t timer, uint32_t new_period, uint32_t block_ticks);

/**
 * @brief 获取定时器创建时绑定的用户参数。
 * @param timer [in] 定时器句柄。
 * @return void* 存储在定时器中的用户参数指针。
 */
void *Timer_GetArg(TimerHandle_t timer);

#endif // MYRTOS_TIMER_ENABLE

#endif // MYRTOS_EXT_TIMER_H
