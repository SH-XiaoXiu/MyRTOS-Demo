//
// GD32F407 Demo Platform Header
//
// 定义平台层的 API 和钩子函数
//

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include "gd32f4xx.h"
#include "platform_config.h"

// 引入 MyRTOS 核心类型，以便钩子函数可以使用
#include "MyRTOS.h"

// ============================================================================
//                           错误处理动作定义
// ============================================================================
typedef enum {
    PLATFORM_ERROR_ACTION_CONTINUE,  // 默认行为: 继续执行，内核将删除出错任务
    PLATFORM_ERROR_ACTION_HALT,      // 请求挂起: 系统进入无限循环
    PLATFORM_ERROR_ACTION_REBOOT     // 请求重启: 调用 NVIC_SystemReset()
} PlatformErrorAction_t;

// ============================================================================
//                           硬件初始化 API
// ============================================================================
/**
 * @brief 初始化平台硬件
 *        包括：系统时钟、NVIC、控制台、高精度定时器等
 */
void Platform_HwInit(void);

/**
 * @brief 重启系统
 */
void Platform_Reboot(void);

// ============================================================================
//                           控制台服务 API
// ============================================================================
#if (PLATFORM_USE_CONSOLE == 1)
#include "MyRTOS_IO.h"

/**
 * @brief 初始化控制台硬件
 */
void Platform_Console_HwInit(void);

/**
 * @brief 初始化控制台的 OS 相关部分（信号量等）
 */
void Platform_Console_OSInit(void);

/**
 * @brief 获取控制台流句柄
 * @return StreamHandle_t 控制台流
 */
StreamHandle_t Platform_Console_GetStream(void);
#endif

// ============================================================================
//                           高精度定时器 API
// ============================================================================
#if (PLATFORM_USE_HIRES_TIMER == 1)
/**
 * @brief 初始化高精度定时器
 */
void Platform_HiresTimer_Init(void);

/**
 * @brief 获取高精度定时器值
 * @return uint32_t 计数值
 */
uint32_t Platform_Timer_GetHiresValue(void);
#endif

// ============================================================================
//                           平台钩子函数（用户可重写）
// ============================================================================

/**
 * @brief 早期初始化钩子（时钟初始化后，其他外设初始化前）
 */
void Platform_EarlyInit_Hook(void);

/**
 * @brief BSP 初始化钩子（平台驱动初始化后）
 */
void Platform_BSP_Init_Hook(void);

/**
 * @brief BSP 初始化后钩子
 */
void Platform_BSP_After_Hook(void);

/**
 * @brief 应用设置钩子（服务初始化后，创建任务前）
 */
void Platform_AppSetup_Hook(void);

/**
 * @brief 创建任务钩子（调度器启动前）
 */
void Platform_CreateTasks_Hook(void);

/**
 * @brief 空闲任务钩子
 */
void Platform_IdleTask_Hook(void *pv);

/**
 * @brief HardFault 处理钩子
 * @param pulFaultStackAddress 故障栈指针
 */
void Platform_HardFault_Hook(uint32_t *pulFaultStackAddress);

/**
 * @brief 栈溢出处理钩子
 * @param pxTask 溢出的任务句柄
 */
void Platform_StackOverflow_Hook(TaskHandle_t pxTask);

/**
 * @brief 内存分配失败钩子
 * @param wantedSize 请求的大小
 */
void Platform_MallocFailed_Hook(size_t wantedSize);

/**
 * @brief 任务退出钩子
 * @param pxTask 退出的任务
 * @return PlatformErrorAction_t 处理动作
 */
PlatformErrorAction_t Platform_TaskExit_Hook(TaskHandle_t pxTask);

// ============================================================================
//                           内部函数（供 hooks 使用）
// ============================================================================
/**
 * @brief 在 fault 上下文中输出字符（非阻塞）
 */
void Platform_fault_putchar(char c);

#endif // PLATFORM_H
