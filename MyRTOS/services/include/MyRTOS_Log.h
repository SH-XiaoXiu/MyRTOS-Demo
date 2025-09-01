#ifndef MYRTOS_LOG_H
#define MYRTOS_LOG_H

#include "MyRTOS_Service_Config.h"
#include "MyRTOS_IO.h"

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)

// 日志等级枚举
typedef enum {
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
} LogLevel_t;

// --- 公共 API ---
int Log_Init(LogLevel_t initial_level, StreamHandle_t output_stream);

void Log_SetLevel(LogLevel_t level);

#ifndef MYRTOS_LOG_MAX_LEVEL
#define MYRTOS_LOG_MAX_LEVEL LOG_LEVEL_DEBUG
#endif

#ifndef MYRTOS_LOG_FORMAT
#define MYRTOS_LOG_FORMAT "[%5llu][%c][%s]"
#endif


// --- 日志打印宏 (给应用开发者使用的便捷接口) ---
#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_ERROR)
#define LOG_E(tag, format, ...) Log_Printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#else
#define LOG_E(tag, format, ...) do {} while(0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_WARN)
#define LOG_W(tag, format, ...) Log_Printf(LOG_LEVEL_WARN,  tag, format, ##__VA_ARGS__)
#else
#define LOG_W(tag, format, ...) do {} while(0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_INFO)
#define LOG_I(tag, format, ...) Log_Printf(LOG_LEVEL_INFO,  tag, format, ##__VA_ARGS__)
#else
#define LOG_I(tag, format, ...) do {} while(0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_DEBUG)
#define LOG_D(tag, format, ...) Log_Printf(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#else
#define LOG_D(tag, format, ...) do {} while(0)
#endif

// --- 底层打印函数 (由宏调用) ---
void Log_Printf(LogLevel_t level, const char *tag, const char *format, ...);

#else // 如果服务被禁用
#define Log_Init(level, stream) (0)
#define Log_SetLevel(level)
#define LOG_E(tag, format, ...)
#define LOG_W(tag, format, ...)
#define LOG_I(tag, format, ...)
#define LOG_D(tag, format, ...)
#endif // MYRTOS_SERVICE_LOG_ENABLE

#endif // MYRTOS_LOG_H
