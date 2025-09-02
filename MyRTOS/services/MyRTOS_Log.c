#include "MyRTOS_Log.h"

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)

#include <stdio.h>
#include <string.h>
#include "MyRTOS_Port.h"

#ifndef MYRTOS_LOG_QUEUE_LENGTH
#define MYRTOS_LOG_QUEUE_LENGTH 64
#endif
#ifndef MYRTOS_LOG_TASK_STACK_SIZE
#define MYRTOS_LOG_TASK_STACK_SIZE 2048
#endif
#ifndef MYRTOS_LOG_TASK_PRIORITY
#define MYRTOS_LOG_TASK_PRIORITY (MYRTOS_MAX_PRIORITIES - 1) // 默认高优先级
#endif
#ifndef MYRTOS_LOG_FORMAT
#define MYRTOS_LOG_FORMAT "[%5llu][%c][%s][%s] "
#endif

// 服务的内部状态
static LogLevel_t g_log_level = LOG_LEVEL_DEBUG;
static QueueHandle_t g_io_request_queue;

// 任务实现
static void IOServer_Task(void *param) {
    // 重命名为更通用的名字
    (void) param;
    AsyncWriteRequest_t request;
    for (;;) {
        if (Queue_Receive(g_io_request_queue, &request, MYRTOS_MAX_DELAY) == 1) {
            if (request.target_stream) {
                // 执行真正的写入操作
                Stream_Write(request.target_stream, request.message, strlen(request.message), MYRTOS_MAX_DELAY);
            }
        }
    }
}

// 公共API实现
int Log_Init(void) {
    g_io_request_queue = Queue_Create(MYRTOS_LOG_QUEUE_LENGTH, sizeof(AsyncWriteRequest_t));
    if (!g_io_request_queue)
        return -1;

    TaskHandle_t task_h =
            Task_Create(IOServer_Task, "IOServer", MYRTOS_LOG_TASK_STACK_SIZE, NULL, MYRTOS_LOG_TASK_PRIORITY);
    if (!task_h) {
        Queue_Delete(g_io_request_queue);
        g_io_request_queue = NULL;
        return -1;
    }
    return 0;
}

void Log_SetLevel(LogLevel_t level) {
    MyRTOS_Port_EnterCritical();
    g_log_level = level;
    MyRTOS_Port_ExitCritical();
}

void Log_Printf(LogLevel_t level, const char *tag, const char *format, ...) {
    if (g_io_request_queue == NULL || level > g_log_level) {
        return;
    }

    AsyncWriteRequest_t request;
    request.target_stream = Task_GetStdOut(Task_GetCurrentTaskHandle());
    if (!request.target_stream) {
        return;
    }

    const char *task_name = Task_GetName(Task_GetCurrentTaskHandle());
    if (task_name == NULL)
        task_name = "NoName";

    char level_char = ' ';
    switch (level) {
        case LOG_LEVEL_ERROR:
            level_char = 'E';
            break;
        case LOG_LEVEL_WARN:
            level_char = 'W';
            break;
        case LOG_LEVEL_INFO:
            level_char = 'I';
            break;
        case LOG_LEVEL_DEBUG:
            level_char = 'D';
            break;
        default:
            break;
    }

    int header_len = snprintf(request.message, sizeof(request.message), MYRTOS_LOG_FORMAT, MyRTOS_GetTick(), level_char,
                              task_name, tag);

    if (header_len < 0 || header_len >= sizeof(request.message)) {
        header_len = 0;
    }

    va_list args;
    va_start(args, format);
    int body_len = vsnprintf(request.message + header_len, sizeof(request.message) - header_len, format, args);
    va_end(args);

    if (body_len < 0)
        body_len = 0;

    int total_len = header_len + body_len;
    if (total_len < (sizeof(request.message) - 2)) {
        request.message[total_len] = '\n';
        request.message[total_len + 1] = '\0';
    } else {
        request.message[sizeof(request.message) - 2] = '\n';
        request.message[sizeof(request.message) - 1] = '\0';
    }

    Queue_Send(g_io_request_queue, &request, 0);
}


void MyRTOS_AsyncVprintf(StreamHandle_t stream, const char *format, va_list args) {
    if (!g_io_request_queue || !stream)
        return;

    AsyncWriteRequest_t request;
    request.target_stream = stream;
    int len = vsnprintf(request.message, sizeof(request.message), format, args);

    // 确保以空字符结尾
    if (len >= sizeof(request.message)) {
        request.message[sizeof(request.message) - 1] = '\0';
    }

    Queue_Send(g_io_request_queue, &request, 0);
}

void MyRTOS_AsyncPrintf(StreamHandle_t stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    MyRTOS_AsyncVprintf(stream, format, args);
    va_end(args);
}

#endif // MYRTOS_SERVICE_LOG_ENABLE
