//
// Created by XiaoXiu on 8/28/2025.
//
#include "MyRTOS.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if (MY_RTOS_USE_LOG == 1)

// --- 内部状态 ---
static QueueHandle_t g_log_queue = NULL;
static TaskHandle_t g_log_task_handle = NULL;
static volatile int g_is_monitor_active = 0; // 0: Normal, 1: Monitor

// --- 内部函数原型 ---
static void prvLogWriterTask(void *pv);

static void prvSendToLogQueue(const char *str);


/**
 * @brief 初始化日志系统
 */
void MyRTOS_Log_Init(void) {
    if (g_log_queue) return;
    g_log_queue = Queue_Create(SYS_LOG_QUEUE_LENGTH, SYS_LOG_MAX_MSG_LENGTH);
    if (!g_log_queue) {
        /* Handle fatal error */
        return;
    }
    g_log_task_handle = Task_Create(prvLogWriterTask, "SYS_LOG", SYS_LOG_TASK_STACK_SIZE, NULL, SYS_LOG_TASK_PRIORITY);
    if (!g_log_task_handle) {
        /* Handle fatal error */
    }
}

/**
 * @brief 日志任务 (唯一的消费者)
 */
static void prvLogWriterTask(void *pv) {
    char log_buffer[SYS_LOG_MAX_MSG_LENGTH];
    while (1) {
        if (Queue_Receive(g_log_queue, log_buffer, MY_RTOS_MAX_DELAY)) {
            // 如果不在监视器模式，就打印
            if (!g_is_monitor_active) {
                const char *ptr = log_buffer;
                while (*ptr) MyRTOS_Platform_PutChar(*ptr++);
            }
        }
    }
}

/**
 * @brief 将格式化好的字符串发送到队列的统一入口
 */
static void prvSendToLogQueue(const char *str) {
    if (g_log_queue) {
        // 使用0超时，如果队列满了就丢弃，绝不阻塞
        Queue_Send(g_log_queue, str, 0);
    }
}

/**
 * @brief PRINT(...) 的后端实现
 */
void MyRTOS_Log_Printf(const char *fmt, ...) {
    char temp_buffer[SYS_LOG_MAX_MSG_LENGTH];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp_buffer, sizeof(temp_buffer), fmt, args);
    va_end(args);

    prvSendToLogQueue(temp_buffer);
}

/**
 * @brief SYS_LOGx(...) 的后端实现
 */
void MyRTOS_Log_Vprintf(int level, const char *file, int line, const char *fmt, ...) {
    if (level > SYS_LOG_LEVEL) return; // 正确的日志级别检查

    char temp_buffer[SYS_LOG_MAX_MSG_LENGTH];
    char *p = temp_buffer;
    int len = 0;
    static const char *level_map = " EWID"; // " " for NONE

    // 格式化前缀
    len += snprintf(p, sizeof(temp_buffer), "[%c][%llu] %s:%d: ",
                    level_map[level], MyRTOS_GetTick(), file, line);

    // 格式化用户消息
    va_list args;
    va_start(args, fmt);
    len += vsnprintf(p + len, sizeof(temp_buffer) - len, fmt, args);
    va_end(args);

    // 添加换行
    if (len < sizeof(temp_buffer) - 2) {
        p[len++] = '\r';
        p[len++] = '\n';
        p[len] = '\0';
    } else {
        // 确保安全结尾
        strncpy(p + sizeof(temp_buffer) - 5, "...\r\n", 5);
    }
    if (g_log_queue == NULL) {
        const char *ptr = temp_buffer;
        while (*ptr) {
            MyRTOS_Platform_PutChar(*ptr++);
        }
        return;
    }
    prvSendToLogQueue(temp_buffer);
}


// --- 为监视器提供的控制接口 ---
void MyRTOS_Log_SetMonitorMode(int active) {
    g_is_monitor_active = active;
}

#endif // MY_RTOS_USE_LOG
