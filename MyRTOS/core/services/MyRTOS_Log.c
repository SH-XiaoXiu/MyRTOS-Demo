//
// Created by XiaoXiu on 8/28/2025.
//

#include "MyRTOS.h"
#include "MyRTOS_Log.h"

#if (MY_RTOS_USE_LOG == 1)

#include "MyRTOS_IO.h"
#include "MyRTOS_Platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static QueueHandle_t g_log_queue = NULL;
static TaskHandle_t g_log_task_handle = NULL;
static int g_system_log_level = SYS_LOG_LEVEL;
static int g_user_log_level = USER_LOG_LEVEL;

static void prvLogWriterTask(void *pv);

static void prvSendToLogQueue(const char *str, int len);

static void prvKernelLogHook(const char *message, uint16_t length);

/**
 * @brief 初始化日志系统
 */
void MyRTOS_Log_Init(void) {
    if (g_log_queue) return;

    g_log_queue = Queue_Create(SYS_LOG_QUEUE_LENGTH, SYS_LOG_MAX_MSG_LENGTH);
    if (!g_log_queue) {
        // 在这个阶段，StdIO还未完全就绪，使用最原始的方式打印
        const char *err_msg = "Error: Failed to create log queue.\n";
        for (const char *p = err_msg; *p; ++p) MyRTOS_Platform_PutChar(*p);
        return;
    }

    g_log_task_handle = Task_Create(prvLogWriterTask, "LogWriter", SYS_LOG_TASK_STACK_SIZE, NULL,
                                    SYS_LOG_TASK_PRIORITY);
    if (!g_log_task_handle) {
        // 处理任务创建失败
        const char *err_msg = "Error: Failed to create log task.\n";
        for (const char *p = err_msg; *p; ++p) MyRTOS_Platform_PutChar(*p);
    }
    // 将日志钩子函数注册到内核
    MyRTOS_RegisterKernelLogHook(prvKernelLogHook);
}

/**
 * @brief 日志任务 (唯一的消费者)
 *        它从日志队列中取出消息，并将其打印到标准错误流。
 */
static void prvLogWriterTask(void *pv) {
    char log_buffer[SYS_LOG_MAX_MSG_LENGTH];
    while (1) {
        // 无限期阻塞，等待新的日志消息
        if (Queue_Receive(g_log_queue, log_buffer, MY_RTOS_MAX_DELAY)) {
            // 将日志输出到标准错误流
            //后面可以将日志与常规输出分离
            MyRTOS_fprintf(g_myrtos_std_err, "%s", log_buffer);
        }
    }
}

/**
 * @brief 将格式化好的字符串发送到队列的统一入口
 */
static void prvSendToLogQueue(const char *str, int len) {
    if (g_log_queue && len < SYS_LOG_MAX_MSG_LENGTH) {
        // 使用0超时，如果队列满了就丢弃日志，绝不阻塞调用者任务
        Queue_Send(g_log_queue, str, 0);
    }
}

/**
 * @brief 这是注册给内核的钩子函数，用于处理内核日志。
 */
static void prvKernelLogHook(const char *message, uint16_t length) {
    // 内核日志被视为系统的DEBUG级别
    if (g_system_log_level < SYS_LOG_LEVEL_DEBUG) {
        return;
    }
    char temp_buffer[SYS_LOG_MAX_MSG_LENGTH];
    // 格式化内核日志，为其添加前缀
    int len = snprintf(temp_buffer, sizeof(temp_buffer), "[S][D][KERNEL] %.*s\n", length, message);
    if (len > 0) {
        // 同样将格式化后的内核日志发送到队列
        prvSendToLogQueue(temp_buffer, len);
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
    int len = 0;

    static const char *level_map = " EWID"; // NONE, ERROR, WARN, INFO, DEBUG
    char module_char = (module == LOG_MODULE_SYSTEM) ? 'S' : 'U';

    // 格式化日志前缀
    len += snprintf(temp_buffer + len, sizeof(temp_buffer) - len, "[%c][%c][%llu] ",
                    module_char, level_map[level], MyRTOS_GetTick());

    // 如果提供了文件名和行号，添加它们
    if (file != NULL && line > 0) {
        len += snprintf(temp_buffer + len, sizeof(temp_buffer) - len, "%s:%d: ", file, line);
    }

    // 格式化用户消息
    va_list args;
    va_start(args, fmt);
    len += vsnprintf(temp_buffer + len, sizeof(temp_buffer) - len, fmt, args);
    va_end(args);

    // 确保以换行符结尾
    if (len < (int) sizeof(temp_buffer) - 1) {
        temp_buffer[len++] = '\n';
        temp_buffer[len] = '\0';
    } else {
        // 缓冲区满了，强制在末尾添加换行符
        temp_buffer[sizeof(temp_buffer) - 2] = '\n';
        temp_buffer[sizeof(temp_buffer) - 1] = '\0';
        len = sizeof(temp_buffer);
    }

    // 关键逻辑：判断如何输出
    if (!MyRTOS_Schedule_IsRunning() || g_log_queue == NULL) {
        // 调度器未运行或队列创建失败：直接裸机输出
        // MyRTOS_fprintf 内部会自动处理这种情况
        MyRTOS_fprintf(NULL, "%s", temp_buffer);
    } else {
        // 正常情况：发送到日志队列
        prvSendToLogQueue(temp_buffer, len);
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
