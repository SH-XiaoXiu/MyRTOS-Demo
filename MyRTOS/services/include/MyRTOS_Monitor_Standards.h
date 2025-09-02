/**
 * @file  MyRTOS_Monitor_Standards.h
 * @brief MyRTOS 监控服务 - 标准定义
 * @details 定义了监控服务所需的一些标准类型和函数原型，用于解耦。
 */
#ifndef MYRTOS_MONITOR_STANDARDS_H
#define MYRTOS_MONITOR_STANDARDS_H

#include "MyRTOS_Service_Config.h"

#ifndef MYRTOS_SERVICE_MONITOR_ENABLE
#define MYRTOS_SERVICE_MONITOR_ENABLE 0
#endif

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1

#include <stdint.h>

/**
 * @brief 获取高精度定时器计数值的函数原型。
 * @details 此函数应由平台抽象层(PAL)或BSP层实现，并通过依赖注入的方式提供给监控模块。
 *          用于精确计算任务运行时间。
 * @return uint32_t 当前高精度定时器的计数值。
 */
typedef uint32_t (*MonitorGetHiresTimerValueFn)(void);

#endif

#endif
