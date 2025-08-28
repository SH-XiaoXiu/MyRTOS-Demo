#ifndef MYRTOS_LOG_H
#define MYRTOS_LOG_H

#include "MyRTOS_Config.h"
#include <stdio.h>

#if (MY_RTOS_USE_LOG == 1)

// 初始化日志系统 (创建日志任务和队列)
void MyRTOS_Log_Init(void);

// 内部实现函数
void MyRTOS_Log_Printf(const char* fmt, ...);

// --- 用户接口 ---
#define PRINT(...) MyRTOS_Log_Printf(__VA_ARGS__)

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// 内部实现函数
void MyRTOS_Log_Vprintf(int level, const char* file, int line, const char* fmt, ...);

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_ERROR)
#define SYS_LOGE(fmt, ...) MyRTOS_Log_Vprintf(SYS_LOG_LEVEL_ERROR, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define SYS_LOGE(fmt, ...)
#endif

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_WARN)
#define SYS_LOGW(fmt, ...) MyRTOS_Log_Vprintf(SYS_LOG_LEVEL_WARN, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define SYS_LOGW(fmt, ...)
#endif

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_INFO)
#define SYS_LOGI(fmt, ...) MyRTOS_Log_Vprintf(SYS_LOG_LEVEL_INFO, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define SYS_LOGI(fmt, ...)
#endif

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_DEBUG)
#define SYS_LOGD(fmt, ...) MyRTOS_Log_Vprintf(SYS_LOG_LEVEL_DEBUG, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define SYS_LOGD(fmt, ...)
#endif

#else
// 禁用所有日志和打印
#define MyRTOS_Log_Init()
#define PRINT(...)
#define SYS_LOGE(fmt, ...)
#define SYS_LOGW(fmt, ...)
#define SYS_LOGI(fmt, ...)
#define SYS_LOGD(fmt, ...)
#endif

#endif