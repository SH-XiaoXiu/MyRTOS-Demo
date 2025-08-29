//
// Created by XiaoXiu on 8/28/2025.
//
#include "MyRTOS.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Platform.h"
#include "MyRTOS_Console.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if (MY_RTOS_USE_LOG == 1)

static QueueHandle_t g_log_queue = NULL;
static TaskHandle_t g_log_task_handle = NULL;
static int g_system_log_level = SYS_LOG_LEVEL;
static int g_user_log_level = USER_LOG_LEVEL;

static void prvLogWriterTask(void *pv);

static void prvSendToLogQueue(const char *str);


/**
 * @brief 初始化日志系统
 */
void MyRTOS_Log_Init(void) {
    if (g_log_queue) return;
    g_log_queue = Queue_Create(SYS_LOG_QUEUE_LENGTH, SYS_LOG_MAX_MSG_LENGTH);
    if (!g_log_queue) {
        SYS_LOGE("Failed to create log queue\n");
        return;
    }
    g_log_task_handle = Task_Create(prvLogWriterTask, "SYS_LOG", SYS_LOG_TASK_STACK_SIZE, NULL, SYS_LOG_TASK_PRIORITY);
    if (!g_log_task_handle) {
        SYS_LOGE("Failed to create log task\n");
    }
}

/**
 * @brief 日志任务 (唯一的消费者)
 */
static void prvLogWriterTask(void *pv) {
    char log_buffer[SYS_LOG_MAX_MSG_LENGTH];
    while (1) {
        // 持续消费日志队列
        if (Queue_Receive(g_log_queue, log_buffer, MY_RTOS_MAX_DELAY)) {
            if (MyRTOS_Console_GetMode() == CONSOLE_MODE_LOG) {
                MyRTOS_Console_Printf("%s", log_buffer);
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
 * @brief SYS_LOGx(...) 和 USER_LOGx(...) 的后端实现
 */
void MyRTOS_Log_Vprintf(LogModule_t module, int level, const char *file, int line, const char *fmt, ...) {
    if ((module == LOG_MODULE_SYSTEM && level > g_system_log_level) ||
        (module == LOG_MODULE_USER && level > g_user_log_level)) {
        return;
    }

    char temp_buffer[SYS_LOG_MAX_MSG_LENGTH];
    char *p = temp_buffer;
    int len = 0;
    int remaining_size = sizeof(temp_buffer);

    static const char *level_map = " EWID";
    char module_char = (module == LOG_MODULE_SYSTEM) ? 'S' : 'U';

    if (file != NULL) {
        // 如果提供了文件名，打印完整的前缀
        len += snprintf(p + len, remaining_size - len, "[%c][%c][%llu] %s:%d: ",
                        module_char, level_map[level], MyRTOS_GetTick(), file, line);
    } else {
        // 如果文件名是 NULL，打印简化的前缀
        len += snprintf(p + len, remaining_size - len, "[%c][%c][%llu] ",
                        module_char, level_map[level], MyRTOS_GetTick());
    }

    //格式化用户消息
    va_list args;
    va_start(args, fmt);
    // 确保还有空间可写
    if (len < remaining_size) {
        len += vsnprintf(p + len, remaining_size - len, fmt, (va_list)args);
    }
    va_end(args);

    // 添加换行并确保安全结尾
    if (len < remaining_size - 1) {
        p[len++] = '\n';
        p[len] = '\0';
    } else {
        if (remaining_size > 1) {
            p[remaining_size - 2] = '\n';
            p[remaining_size - 1] = '\0';
        }
    }

    if (!MyRTOS_Schedule_IsRunning() || g_log_queue == NULL) {
        const char *ptr = temp_buffer;
        while (*ptr) MyRTOS_Platform_PutChar(*ptr++);
    } else {
        prvSendToLogQueue(temp_buffer);
    }
}

void MyRTOS_Log_SetLevel(LogModule_t module, int level) {
    MyRTOS_Port_ENTER_CRITICAL();
    if (module == LOG_MODULE_SYSTEM) {
        g_system_log_level = level;
    } else {
        g_user_log_level = level;
    }
    MyRTOS_Port_EXIT_CRITICAL();
}

#endif // MY_RTOS_USE_LOG
