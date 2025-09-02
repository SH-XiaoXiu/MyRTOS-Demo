/**
 * @file  MyRTOS_Log.h
 * @brief MyRTOS 日志服务与异步IO
 */
#ifndef MYRTOS_LOG_H
#define MYRTOS_LOG_H

#include <stdarg.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Service_Config.h"

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)

// 日志等级枚举
typedef enum {
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
} LogLevel_t;

#ifndef MYRTOS_LOG_MSG_MAX_SIZE
#define MYRTOS_LOG_MSG_MAX_SIZE 128
#endif


// 异步IO请求结构体
typedef struct {
    StreamHandle_t target_stream; // 目标流
    char message[MYRTOS_LOG_MSG_MAX_SIZE]; // 消息内容
} AsyncWriteRequest_t;


// 公共 API

/**
 * @brief 初始化异步日志/IO服务。
 * @details 该函数会创建一个高优先级的后台任务来处理所有的打印请求，
 *          确保调用打印函数的业务任务不会被IO阻塞。
 * @return int 0 表示成功, -1 表示失败。
 */
int Log_Init(void);

/**
 * @brief 设置全局日志过滤等级。
 * @param level [in] 新的日志等级。低于此等级的日志将被忽略。
 */
void Log_SetLevel(LogLevel_t level);

// 日志打印宏 (给应用开发者使用的便捷接口)

#ifndef MYRTOS_LOG_MAX_LEVEL
#define MYRTOS_LOG_MAX_LEVEL LOG_LEVEL_DEBUG
#endif

// 日志宏现在调用 Log_Printf，它内部会使用异步机制
#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_ERROR)
#define LOG_E(tag, format, ...) Log_Printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#else
#define LOG_E(tag, format, ...)                                                                                        \
    do {                                                                                                               \
    } while (0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_WARN)
#define LOG_W(tag, format, ...) Log_Printf(LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#else
#define LOG_W(tag, format, ...)                                                                                        \
    do {                                                                                                               \
    } while (0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_INFO)
#define LOG_I(tag, format, ...) Log_Printf(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#else
#define LOG_I(tag, format, ...)                                                                                        \
    do {                                                                                                               \
    } while (0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_DEBUG)
#define LOG_D(tag, format, ...) Log_Printf(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#else
#define LOG_D(tag, format, ...)                                                                                        \
    do {                                                                                                               \
    } while (0)
#endif

// 底层打印函数

/**
 * @brief (底层函数) 供日志宏调用，将格式化的日志消息作为IO请求发送到队列。
 */
void Log_Printf(LogLevel_t level, const char *tag, const char *format, ...);

/**
 * @brief (底层函数) 异步地向指定流打印格式化字符串 (va_list版本)。
 */
void MyRTOS_AsyncVprintf(StreamHandle_t stream, const char *format, va_list args);

/**
 * @brief 异步地向指定流打印格式化字符串。
 */
void MyRTOS_AsyncPrintf(StreamHandle_t stream, const char *format, ...);

// MyRTOS_printf 现在应该也变成异步的，以保持系统行为一致
#undef MyRTOS_printf
#define MyRTOS_printf(format, ...) MyRTOS_AsyncPrintf(Task_GetStdOut(NULL), format, ##__VA_ARGS__)

#else // 如果服务被禁用
#define Log_Init() (0)
#define Log_SetLevel(level)
#define LOG_E(tag, format, ...)
#define LOG_W(tag, format, ...)
#define LOG_I(tag, format, ...)
#define LOG_D(tag, format, ...)
#define MyRTOS_printf(format, ...) // 如果日志服务禁用，printf也应被禁用或重定向到同步版本
#endif // MYRTOS_SERVICE_LOG_ENABLE

#endif // MYRTOS_LOG_H
