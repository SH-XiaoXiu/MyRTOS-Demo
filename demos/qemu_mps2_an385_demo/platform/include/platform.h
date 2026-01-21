/**
 * @brief QEMU MPS2-AN385 平台层头文件
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include "MyRTOS_Config.h"
#include "MyRTOS_Service_Config.h"
#include "MyRTOS.h"

#if MYRTOS_SERVICE_IO_ENABLE == 1
#include "MyRTOS_IO.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
//                           平台配置
// ============================================================================

#define PLATFORM_USE_CONSOLE        1
#define PLATFORM_USE_HIRES_TIMER    1
#define PLATFORM_HIRES_TIMER_NUM    0

// ============================================================================
//                           错误处理动作定义
// ============================================================================

typedef enum {
    PLATFORM_ERROR_ACTION_CONTINUE,  // 默认行为: 继续执行，内核将删除出错任务
    PLATFORM_ERROR_ACTION_HALT,      // 请求挂起: 系统进入无限循环
    PLATFORM_ERROR_ACTION_REBOOT     // 请求重启: 调用 NVIC_SystemReset()
} PlatformErrorAction_t;

// ============================================================================
//                           平台初始化
// ============================================================================

/**
 * @brief 平台硬件初始化
 *        配置时钟、UART、定时器等
 */
void Platform_HwInit(void);

/**
 * @brief 重启系统
 */
void Platform_Reboot(void);

// ============================================================================
//                           控制台接口
// ============================================================================

#if PLATFORM_USE_CONSOLE == 1

/**
 * @brief 控制台硬件初始化
 */
void Platform_Console_HwInit(void);

/**
 * @brief 控制台 OS 部分初始化
 *        创建信号量等同步原语
 */
void Platform_Console_OSInit(void);

/**
 * @brief 获取控制台流句柄
 */
StreamHandle_t Platform_Console_GetStream(void);

#endif

// ============================================================================
//                           高精度定时器接口
// ============================================================================

#if PLATFORM_USE_HIRES_TIMER == 1

/**
 * @brief 初始化高精度定时器
 */
void Platform_HiresTimer_Init(void);

/**
 * @brief 获取高精度定时器当前值
 */
uint32_t Platform_Timer_GetHiresValue(void);

#endif

// ============================================================================
//                           平台钩子函数
// ============================================================================

/**
 * @brief 早期初始化钩子（时钟配置后，外设初始化前）
 */
void Platform_EarlyInit_Hook(void);

/**
 * @brief BSP 初始化钩子（外设初始化时）
 */
void Platform_BSP_Init_Hook(void);

/**
 * @brief BSP 初始化完成钩子
 */
void Platform_BSP_After_Hook(void);

/**
 * @brief 空闲任务钩子
 */
void Platform_IdleTask_Hook(void *pv);

/**
 * @brief 硬故障钩子
 */
void Platform_HardFault_Hook(uint32_t *pulFaultStackAddress);

/**
 * @brief 栈溢出钩子
 */
void Platform_StackOverflow_Hook(TaskHandle_t pxTask);

/**
 * @brief 内存分配失败钩子
 */
void Platform_MallocFailed_Hook(size_t wantedSize);

/**
 * @brief 任务退出钩子
 * @return PlatformErrorAction_t 处理动作
 */
PlatformErrorAction_t Platform_TaskExit_Hook(TaskHandle_t pxTask);

// ============================================================================
//                           错误处理
// ============================================================================

/**
 * @brief 初始化错误处理器
 */
void Platform_ErrorHandler_Init(void);

/**
 * @brief 在 fault 上下文中输出字符（非阻塞）
 */
void Platform_fault_putchar(char c);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
