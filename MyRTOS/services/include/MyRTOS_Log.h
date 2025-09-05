/**
 * @file    MyRTOS_Log.h
 * @brief   MyRTOS 日志框架 - 公共接口
 * @details 重构的日志框架 遵守了职责分离原则 之前的混在一起的一整坨现在进行清晰的分离和重构
 */
#ifndef MYRTOS_LOG_H
#define MYRTOS_LOG_H

#include "MyRTOS_Service_Config.h"

#if MYRTOS_SERVICE_LOG_ENABLE == 1

#include "MyRTOS.h"
#include "MyRTOS_IO.h"


/**
 * @brief 日志等级
 */
typedef enum {
    LOG_LEVEL_NONE = 0, // 不输出任何日志
    LOG_LEVEL_ERROR,    // 关键错误，系统可能无法恢复
    LOG_LEVEL_WARN,     // 警告，可能出现的问题
    LOG_LEVEL_INFO,     // 关键流程信息
    LOG_LEVEL_DEBUG,    // 调试信息
} LogLevel_t;

/**
 * @brief 日志监听器句柄 这是一个对外私有的指针 用于唯一标识一个已注册的监听器。
 */
typedef void* LogListenerHandle_t;


// 公共 API

/**
 * @brief   初始化日志框架。
 * @details 必须在使用任何日志功能前调用
 * @return  int 0 表示成功。一般不会爆炸,里面就是做数据初始化
 */
int Log_Init(void);

/**
 * @brief   设置全局日志过滤级别。
 * @details 只有级别高于等于此设置的日志事件才会被处理和分发
 * @param   level 全局日志等级。
 */
void Log_SetGlobalLevel(LogLevel_t level);

/**
 * @brief   获取当前的全局日志过滤级别。
 * @details 此函数主要供LOG_X()宏内部使用
 * @return  LogLevel_t 当前的全局日志等级。
 */
LogLevel_t Log_GetGlobalLevel(void);

/**
 * @brief   添加一个日志监听器。
 * @details 注册一个Stream作为日志的输出目标 同一个Stream可以被多次添加,当然炸了就不怪我
 *
 * @param   stream      日志要输出到的目标流。
 * @param   max_level   此监听器希望接收的最高日志级别。
 *                           例如，设为LOG_LEVEL_INFO，则ERROR, WARN, INFO
 *                           级别的日志会发送给它，但DEBUG不会。
 * @return  LogListenerHandle_t 成功则返回监听器句柄，失败(如列表已满)返回NULL。
 */
LogListenerHandle_t Log_AddListener(StreamHandle_t stream, LogLevel_t max_level, const char* tag_filter);

/**
 * @brief   移除一个日志监听器。
 * @param   listener_h 要移除的监听器句柄
 * @return  int 0 表示成功, -1 表示句柄无效。
 */
int Log_RemoveListener(LogListenerHandle_t listener_h);

// -------------------- 日志输出核心函数 (供宏调用) --------------------

/**
 * @brief   执行日志格式化与分发
 * @warning 应用程序代码应使用LOG_X()宏,而不是直接调用此函数
 */
void Log_Output(LogLevel_t level, const char *tag, const char *format, ...);


/**
 * @brief   日志记录宏
 * @remark 当在任务上下文中使用 LOG_X 宏时，推荐使用 Task_GetName(NULL) 作为 tag 参数 以确保日志来源的可追溯性。
 */

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)

#define LOG_E(tag, format, ...) do { \
if (LOG_LEVEL_ERROR <= Log_GetGlobalLevel()) { \
Log_Output(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__); \
} \
} while(0)

#define LOG_W(tag, format, ...) do { \
if (LOG_LEVEL_WARN <= Log_GetGlobalLevel()) { \
Log_Output(LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__); \
} \
} while(0)

#define LOG_I(tag, format, ...) do { \
if (LOG_LEVEL_INFO <= Log_GetGlobalLevel()) { \
Log_Output(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__); \
} \
} while(0)

#define LOG_D(tag, format, ...) do { \
if (LOG_LEVEL_DEBUG <= Log_GetGlobalLevel()) { \
Log_Output(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__); \
} \
} while(0)

#else // 如果日志服务被禁用，所有宏都变为空操作

#define LOG_E(tag, format, ...) ((void)0)
#define LOG_W(tag, format, ...) ((void)0)
#define LOG_I(tag, format, ...) ((void)0)
#define LOG_D(tag, format, ...) ((void)0)

#endif
#endif
#endif
