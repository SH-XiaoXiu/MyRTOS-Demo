//
// Created by XiaoXiu on 8/29/2025.
//

#ifndef MYRTOS_DRIVER_TIMER_H
#define MYRTOS_DRIVER_TIMER_H

#include <stdint.h>
#include "MyRTOS_Config.h"

// 检查配置宏是否存在，如果不存在则给一个空的默认值，防止编译错误
#ifndef MY_RTOS_TIMER_DEVICE_LIST
#define MY_RTOS_TIMER_DEVICE_LIST
#endif

// 根据配置宏 MY_RTOS_TIMER_DEVICE_LIST 自动生成定时器ID的枚举
#define USE_TIMER(id, dev, param) TIMER_ID_##id,
typedef enum {
    MY_RTOS_TIMER_DEVICE_LIST
    TIMER_ID_MAX,
    TIMER_ID_FORCE_INT = 0x7FFFFFFF // 确保枚举至少是int大小
} TimerID_t;
#undef USE_TIMER // 使用完后立即取消定义

// 定时器句柄 (Opaque handle)
typedef void* TimerHandle_dev_t;

/**
 * @brief 初始化在 MyRTOS_Config.h 中定义的所有定时器
 *        此函数由 MyRTOS_SystemInit 自动调用
 */
void MyRTOS_Timer_Init(void);

/**
 * @brief 根据逻辑ID获取指定的定时器句柄
 * @param id 定时器逻辑ID
 * @return 成功返回句柄，失败返回NULL
 */
TimerHandle_dev_t MyRTOS_Timer_GetHandle(TimerID_t id);

/**
 * @brief 启动一个定时器
 * @param handle 定时器句柄
 * @return 0 成功
 */
int MyRTOS_Timer_Start(TimerHandle_dev_t handle);

/**
 * @brief 停止一个定时器
 * @param handle 定时器句柄
 * @return 0 成功
 */
int MyRTOS_Timer_Stop(TimerHandle_dev_t handle);

/**
 * @brief 获取定时器的当前计数值
 * @param handle 定时器句柄
 * @return 32位计数值
 */
uint32_t MyRTOS_Timer_GetCount(TimerHandle_dev_t handle);

/**
 * @brief 设置定时器的周期 (自动重装载值)
 * @param handle 定时器句柄
 * @param period 周期值
 * @return 0 成功
 */
int MyRTOS_Timer_SetPeriod(TimerHandle_dev_t handle, uint32_t period);

/**
 * @brief 获取定时器的计数频率 (Hz)
 * @param handle 定时器句柄
 * @return 频率 (Hz)
 */
uint32_t MyRTOS_Timer_GetFreq(TimerHandle_dev_t handle);

#endif // MYRTOS_DRIVER_TIMER_H