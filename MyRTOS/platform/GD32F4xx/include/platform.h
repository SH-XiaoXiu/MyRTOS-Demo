//
// Created by XiaoXiu on 8/31/2025.
//

#ifndef PLATFORM_H
#define PLATFORM_H


typedef enum {
    PLATFORM_ERROR_ACTION_CONTINUE,  // 默认行为: 继续执行，内核将删除出错任务，系统其他部分继续运行
    PLATFORM_ERROR_ACTION_HALT,      // 请求挂起: 内核将使系统进入无限循环，挂起整个系统
    PLATFORM_ERROR_ACTION_REBOOT    // 请求重启: 将调用 NVIC_SystemReset()
} PlatformErrorAction_t;

#include <stdint.h>
#include "gd32f4xx.h"
#include "platform_config.h"

// 引入 MyRTOS 核心类型，以便钩子函数可以使用
#include "MyRTOS.h"
#include "MyRTOS_Shell.h"

// =========================================================================
//                         核心平台初始化流程
// =========================================================================
/**
 * @brief 初始化整个平台层和RTOS核心服务。
 *        这是用户在 main 函数中需要调用的第一个函数。
 */
void Platform_Init(void);


/**
 * @brief 启动平台和RTOS调度器。
 *        这个函数会调用 Platform_CreateTasks_Hook 来创建应用任务，
 *        然后启动RTOS调度器。此函数永不返回。
 */
void Platform_StartScheduler(void);


// =========================================================================
//                            平台服务获取接口
// =========================================================================
#if (PLATFORM_USE_CONSOLE == 1)
#include "MyRTOS_IO.h"
/**
 * @brief 获取已初始化的控制台流句柄。
 * @return StreamHandle_t 指向控制台流的句柄，若未使能则返回NULL。
 */
StreamHandle_t Platform_Console_GetStream(void);
#endif

#if (PLATFORM_USE_HIRES_TIMER == 1)

/**
 * @brief 获取高精度定时器的当前计数值。
 * @return uint32_t 32位计数值。
 */
uint32_t Platform_Timer_GetHiresValue(void);
#endif


// =========================================================================
//                            平台控制接口
// =========================================================================
/**
 * @brief 重启系统
 * @details 执行软件复位，重启整个系统
 */
void Platform_Reboot(void);


#if MYRTOS_SERVICE_SHELL_ENABLE == 1
struct ShellCommand_t;
// =========================================================================
//                            Shell 命令注册
// =========================================================================
/**
 * @brief 向平台注册一个或多个用户自定义的Shell命令。
 * @details 用户应该在 main 函数的 Platform_AppSetup_Hook 钩子中调用此函数。
 *          平台层会收集所有注册的命令，并在最后统一初始化Shell服务。
 *          当调用 help 命令时，所有注册的命令都会被显示出来。
 * @param commands      [in] 指向 ShellCommand_t 命令数组的指针。
 * @param command_count [in] 数组中命令的数量。
 * @return int 0 表示成功, -1 表示失败 (例如，命令列表已满)。
 */
int Platform_RegisterShellCommands(const struct ShellCommand_t *commands, size_t command_count);
#endif


// =========================================================================
//                      用户自定义钩子函数 (Weak Hooks)
//               用户可以在自己的代码中重新实现这些函数
// =========================================================================

/**
 * @brief 在平台核心时钟初始化之后，但在其他外设初始化之前调用。
 *        适合用于设置电源、引脚复用等早期配置。
 */
void Platform_EarlyInit_Hook(void);

/**
 * @brief 在所有平台驱动（如USART, TIMER）初始化之后调用。
 *        【推荐】这是用户初始化自己特定硬件（如传感器、LED、按键）的地方。
 */
void Platform_BSP_Init_Hook(void);

/**
* @brief 在所有平台驱动初始化之后，但创建任何应用任务之前调用。
*/
void Platform_BSP_After_Hook();


/**
 * @brief 在RTOS服务（如Log, Shell）初始化之后，但在创建任何应用任务之前调用。
 *        【推荐】这是用户注册自定义Shell命令的地方。
 * @param shell_h Shell服务的句柄（如果Shell服务被使能）。
 */
#if MYRTOS_SERVICE_SHELL_ENABLE == 1
void Platform_AppSetup_Hook(ShellHandle_t shell_h);
#else
void Platform_AppSetup_Hook();
#endif

/**
 * @brief 在调度器启动之前，用于创建所有应用程序任务。
 *        【推荐】这是用户创建自己业务逻辑任务的地方。
 */
void Platform_CreateTasks_Hook(void);

/**
 * @brief RTOS 空闲任务的实现。
 *        用户可以重写此函数以在空闲时执行低功耗操作或其他后台任务。
 */
void Platform_IdleTask_Hook(void *pv);

/**
 * @brief 硬件故障 (HardFault) 的处理器。
 *        默认实现会打印故障信息并挂起系统。用户可以重写以实现自定义行为（如记录日志、重启）。
 * @param pulFaultStackAddress 指向故障发生时堆栈的指针。
 */
void Platform_HardFault_Hook(uint32_t *pulFaultStackAddress);

/**
 * @brief 任务堆栈溢出的处理器。
 *        默认实现会打印任务信息并挂起系统。
 * @param pxTask 发生溢出的任务句柄。
 */
void Platform_StackOverflow_Hook(TaskHandle_t pxTask);


/**
 * @brief 内核内存分配失败的处理器。
 * @param wantedSize 尝试分配但失败的字节数。
 */
void Platform_MallocFailed_Hook(size_t wantedSize);


/**
 * @brief 任务退出的处理器
 * @param pxTask 正在退出的任务句柄,处理完成后,任务最终会被删除。
 */
PlatformErrorAction_t Platform_TaskExit_Hook(TaskHandle_t pxTask);

#endif // PLATFORM_H
