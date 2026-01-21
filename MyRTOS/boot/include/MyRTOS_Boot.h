//
// MyRTOS Boot Module
//
// 可选的标准启动模块，提供一键启动功能
// 用户可以选择使用此模块或自行编写启动流程
//

#ifndef MYRTOS_BOOT_H
#define MYRTOS_BOOT_H

#include "MyRTOS_Config.h"
#include "MyRTOS_Service_Config.h"  // 服务配置宏定义
#include "MyRTOS.h"

// ============================================================================
//                           启动模块配置
// ============================================================================
// 用户在 MyRTOS_Config.h 中定义 MYRTOS_USE_BOOT_MODULE 来控制是否使用

#ifndef MYRTOS_USE_BOOT_MODULE
#define MYRTOS_USE_BOOT_MODULE      0
#endif

#if MYRTOS_USE_BOOT_MODULE == 1

// ============================================================================
//                           启动配置结构体
// ============================================================================
// 用户需要提供此结构体，包含硬件资源的对接

#if MYRTOS_SERVICE_IO_ENABLE == 1
#include "MyRTOS_IO.h"
#endif

/**
 * @brief 启动配置结构体
 *        用户需要填充此结构体，提供硬件资源
 */
typedef struct {
#if MYRTOS_SERVICE_IO_ENABLE == 1
    StreamHandle_t console_stream;              // 控制台流（用于 stdin/stdout/stderr）
#endif

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
    uint32_t (*get_hires_timer_value)(void);    // 高精度定时器获取函数
#endif

    // 可选的钩子函数（如果不提供则使用默认空实现）
    void (*on_kernel_init)(void);               // 内核初始化后回调
    void (*on_services_init)(void);             // 服务初始化后回调
    void (*create_tasks)(void);                 // 创建用户任务
    void (*idle_task)(void *pv);                // 空闲任务实现（可选）
} BootConfig_t;


// ============================================================================
//                              启动 API
// ============================================================================

/**
 * @brief 标准启动流程
 *        初始化内核、服务，启动调度器
 *        此函数永不返回
 *
 * @param config 启动配置，包含硬件资源和回调函数
 */
void MyRTOS_Boot(const BootConfig_t *config);


/**
 * @brief 仅初始化（不启动调度器）
 *        适用于需要在启动前做更多配置的场景
 *
 * @param config 启动配置
 */
void MyRTOS_Boot_Init(const BootConfig_t *config);


/**
 * @brief 启动调度器
 *        在 MyRTOS_Boot_Init 之后调用
 *        此函数永不返回
 *
 * @param config 启动配置（需要 idle_task 和 create_tasks）
 */
void MyRTOS_Boot_Start(const BootConfig_t *config);


#endif // MYRTOS_USE_BOOT_MODULE

#endif // MYRTOS_BOOT_H
