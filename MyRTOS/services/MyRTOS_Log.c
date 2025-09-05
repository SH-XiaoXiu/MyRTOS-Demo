#include "MyRTOS_Log.h"

#if MYRTOS_SERVICE_LOG_ENABLE == 1

#include <stdio.h>
#include <string.h>
#include "MyRTOS_Port.h"
#include "MyRTOS_Utils.h"
#if (MYRTOS_LOG_USE_ASYNC_OUTPUT == 1 && MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1)
#include "MyRTOS_AsyncIO.h"
#else
#define LOG_HAS_ASYNC 0
#endif


/**
 * @brief 内部结构，用于存储一个监听器的完整信息。
 */
typedef struct {
    StreamHandle_t stream;
    LogLevel_t max_level;
    char tag_filter[MYRTOS_LOG_MAX_TAG_LEN];
    uint8_t is_active; // 标记此槽位是否被使用
} LogListener_t;

/**
 * @brief 全局日志框架的状态。
 */
static struct {
    LogLevel_t global_level;
    LogListener_t listeners[MYRTOS_LOG_MAX_LISTENERS];
    uint8_t is_initialized;
} g_log_context;


static MutexHandle_t g_log_format_lock = NULL;

//-- 公共API实现 --

int Log_Init(void) {
    if (g_log_context.is_initialized) {
        return 0;
    }
    memset(&g_log_context, 0, sizeof(g_log_context));
    g_log_context.global_level = MYRTOS_LOG_DEFAULT_LEVEL; // 默认级别
    if (g_log_format_lock == NULL) {
        g_log_format_lock = Mutex_Create();
        if (g_log_format_lock == NULL) {
            return -1; // 锁创建失败
        }
    }
    g_log_context.is_initialized = 1;
    return 0;
}

void Log_SetGlobalLevel(LogLevel_t level) {
    MyRTOS_Port_EnterCritical();
    g_log_context.global_level = level;
    MyRTOS_Port_ExitCritical();
}

LogLevel_t Log_GetGlobalLevel(void) {
    return g_log_context.global_level;
}

LogListenerHandle_t Log_AddListener(StreamHandle_t stream, LogLevel_t max_level, const char *tag_filter) {
    if (!g_log_context.is_initialized || stream == NULL) {
        return NULL;
    }

    LogListenerHandle_t handle = NULL;
    int free_slot = -1;

    MyRTOS_Port_EnterCritical();

    // 寻找空闲的监听器槽位
    for (int i = 0; i < MYRTOS_LOG_MAX_LISTENERS; ++i) {
        if (!g_log_context.listeners[i].is_active) {
            free_slot = i;
            break;
        }
    }

    if (free_slot != -1) {
        LogListener_t *new_listener = &g_log_context.listeners[free_slot];

        new_listener->stream = stream;
        new_listener->max_level = max_level;

        if (tag_filter && tag_filter[0] != '\0') {
            strncpy(new_listener->tag_filter, tag_filter, MYRTOS_LOG_MAX_TAG_LEN - 1);
            new_listener->tag_filter[MYRTOS_LOG_MAX_TAG_LEN - 1] = '\0';
        } else {
            // 设为空字符串, 作为通配符
            new_listener->tag_filter[0] = '\0';
        }

        new_listener->is_active = 1;

        handle = (LogListenerHandle_t) new_listener;
    }

    MyRTOS_Port_ExitCritical();

    return handle;
}

int Log_RemoveListener(LogListenerHandle_t listener_h) {
    if (!g_log_context.is_initialized || listener_h == NULL) {
        return -1;
    }
    LogListener_t *listener_to_remove = (LogListener_t *) listener_h;
    //标记是否找到
    int found = 0;
    MyRTOS_Port_EnterCritical();
    for (int i = 0; i < MYRTOS_LOG_MAX_LISTENERS; ++i) {
        if (&g_log_context.listeners[i] == listener_to_remove) {
            if (listener_to_remove->is_active) {
                // Log_Output 在遍历时会立即跳过这个监听器
                listener_to_remove->is_active = 0;
                //清理槽位数据
                listener_to_remove->stream = NULL;
                listener_to_remove->max_level = LOG_LEVEL_NONE;
                // 清空tag缓冲区
                memset(listener_to_remove->tag_filter, 0, MYRTOS_LOG_MAX_TAG_LEN);
                found = 1;
            }
            break;
        }
    }
    MyRTOS_Port_ExitCritical();
    if (found) {
        return 0; // 成功移除
    }
    return -1; // 句柄无效或指向一个非激活的监听器
}

void Log_Output(LogLevel_t level, const char *tag, const char *format, ...) {
    // 检查日志服务是否初始化，或传入的tag/format是否有效
    if (!g_log_context.is_initialized || !tag || !format) {
        return;
    }
    va_list args;
    // 使用静态缓冲区以避免在栈上分配大块内存
    static char formatted_log[MYRTOS_LOG_FORMAT_BUFFER_SIZE];
    // 使用互斥锁保护对共享的 formatted_log 缓冲区的访问，以及后续的IO分发操作
    // 这是为了防止多任务同时写缓冲区和分发日志时产生竞态条件
    if (g_log_format_lock && MyRTOS_Schedule_IsRunning()) {
        Mutex_Lock(g_log_format_lock);
    }
    // 日志格式化
    const char *task_name = Task_GetName(Task_GetCurrentTaskHandle());
    if (task_name == NULL) task_name = "???";
    // 根据日志级别选择对应的字符标识
    char level_char = '?';
    switch (level) {
        case LOG_LEVEL_ERROR: level_char = 'E';
            break;
        case LOG_LEVEL_WARN: level_char = 'W';
            break;
        case LOG_LEVEL_INFO: level_char = 'I';
            break;
        case LOG_LEVEL_DEBUG: level_char = 'D';
            break;
        default: break;
    }

    // 格式化日志头部
    int header_len = snprintf(formatted_log, sizeof(formatted_log), "[%llu][%c][%s][%s] ",
                              MyRTOS_GetTick(), level_char, task_name, tag);

    if (header_len < 0) header_len = 0;
    if (header_len >= sizeof(formatted_log)) header_len = sizeof(formatted_log) - 1;

    // 格式化日志主体内容
    va_start(args, format);
    int body_len = MyRTOS_FormatV(formatted_log + header_len,
                                  sizeof(formatted_log) - header_len,
                                  format, args);
    va_end(args);

    if (body_len < 0) body_len = 0;

    // 安全地添加换行符和字符串终止符
    int total_len = header_len + body_len;
    if (total_len < sizeof(formatted_log) - 2) {
        formatted_log[total_len] = '\n';
        formatted_log[total_len + 1] = '\0';
    } else {
        // 如果内容过长，确保末尾是换行符和终止符
        formatted_log[sizeof(formatted_log) - 2] = '\n';
        formatted_log[sizeof(formatted_log) - 1] = '\0';
    }
    // 遍历所有监听器并分发日志
    for (int i = 0; i < MYRTOS_LOG_MAX_LISTENERS; ++i) {
        LogListener_t listener_copy; // 用于在临界区外安全使用的副本
        uint8_t should_dispatch = 0;
        MyRTOS_Port_EnterCritical();
        if (g_log_context.listeners[i].is_active && level <= g_log_context.listeners[i].max_level) {
            // 检查tag是否匹配 (空字符串为通配符)
            if (g_log_context.listeners[i].tag_filter[0] == '\0' ||
                strcmp(g_log_context.listeners[i].tag_filter, tag) == 0) {
                // 复制监听器信息，以便在临界区外使用
                listener_copy = g_log_context.listeners[i];
                should_dispatch = 1;
            }
        }
        MyRTOS_Port_ExitCritical();
        if (should_dispatch) {
#if MYRTOS_LOG_USE_ASYNC_OUTPUT
            // 如果定义了异步IO，这里会把消息发给异步IO任务处理
            MyRTOS_AsyncPrintf(listener_copy.stream, "%s", formatted_log);
#else
            // 否则，直接同步写入流
            Stream_Printf(listener_copy.stream, "%s", formatted_log);
#endif
        }
    }
    // 所有分发操作完成，释放互斥锁
    if (g_log_format_lock && MyRTOS_Schedule_IsRunning()) {
        Mutex_Unlock(g_log_format_lock);
    }
}

#endif
