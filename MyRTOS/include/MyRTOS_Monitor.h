#ifndef MYRTOS_MONITOR_H
#define MYRTOS_MONITOR_H

#include "MyRTOS_Config.h"

// =============================
// Kernel Event Reporting API (provided by Monitor Service)
// =============================
#if (MY_RTOS_MONITOR_KERNEL_LOG == 1)
    #include "MyRTOS_Log.h"
    #define MY_RTOS_KERNEL_LOGD(fmt, ...)   SYS_LOGD(fmt, ##__VA_ARGS__)
    #define MY_RTOS_KERNEL_LOGE(fmt, ...)   SYS_LOGE(fmt, ##__VA_ARGS__)
#else
    #define MY_RTOS_KERNEL_LOGD(fmt, ...)
    #define MY_RTOS_KERNEL_LOGE(fmt, ...)
#endif


#if (MY_RTOS_USE_MONITOR == 1)

/**
 * @brief 启动系统监视器。
 *        此函数会创建监视器任务，并通知日志系统进入“静音”模式。
 *        监视器任务将接管所有串口输出，以终端形式循环刷新系统状态。
 * @return 0 成功, -1 失败 (如任务已在运行或创建失败).
 */
int MyRTOS_Monitor_Start(void);

/**
 * @brief 停止系统监视器。
 *        此函数会销毁监视器任务并恢复正常的日志I/O输出。
 * @return 0 成功, -1 失败 (如任务未在运行).
 */
int MyRTOS_Monitor_Stop(void);

/**
 * @brief 查询监视器是否正在运行。
 * @return 1 正在运行, 0 已停止.
 */
int MyRTOS_Monitor_IsRunning(void);


#endif // MY_RTOS_USE_MONITOR

#endif // MYRTOS_MONITOR_H