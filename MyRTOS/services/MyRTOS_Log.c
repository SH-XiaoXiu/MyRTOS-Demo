
#include "MyRTOS_Service_Config.h"

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)
#include "MyRTOS_Log.h"
#include "MyRTOS.h"
#include <stdio.h>
#include <string.h>
#include "MyRTOS_Port.h"
#ifndef MYRTOS_LOG_QUEUE_LENGTH
#define MYRTOS_LOG_QUEUE_LENGTH    32
#endif
#ifndef MYRTOS_LOG_MSG_MAX_SIZE
#define MYRTOS_LOG_MSG_MAX_SIZE    128
#endif
#ifndef MYRTOS_LOG_TASK_STACK_SIZE
#define MYRTOS_LOG_TASK_STACK_SIZE 2048
#endif
#ifndef MYRTOS_LOG_TASK_PRIORITY
#define MYRTOS_LOG_TASK_PRIORITY   1
#endif

// --- 服务的内部状态 ---
static QueueHandle_t g_log_queue = NULL;
static StreamHandle_t g_log_output_stream = NULL;
static LogLevel_t g_log_level = LOG_LEVEL_DEBUG;

static void log_task_entry(void *param) {
    (void)param;
    char log_message[MYRTOS_LOG_MSG_MAX_SIZE];
    while (1) {
        if (Queue_Receive(g_log_queue, log_message, MYRTOS_MAX_DELAY) == 1) {
            if (g_log_output_stream) {
                Stream_Write(g_log_output_stream, log_message, strlen(log_message), MYRTOS_MAX_DELAY);
            }
        }
    }
}

int Log_Init(LogLevel_t initial_level, StreamHandle_t output_stream) {
    g_log_level = initial_level;
    g_log_output_stream = output_stream;

    if (g_log_queue == NULL) {
        g_log_queue = Queue_Create(MYRTOS_LOG_QUEUE_LENGTH, MYRTOS_LOG_MSG_MAX_SIZE);
        if (g_log_queue == NULL) return -1;
    }
    
    TaskHandle_t log_task_handle = Task_Create(
        log_task_entry, 
        "LogService",
        MYRTOS_LOG_TASK_STACK_SIZE, 
        NULL, 
        MYRTOS_LOG_TASK_PRIORITY
    );
    
    if (log_task_handle == NULL) {
        Queue_Delete(g_log_queue);
        g_log_queue = NULL;
        return -1;
    }
    
    return 0;
}

void Log_SetLevel(LogLevel_t level) {
    MyRTOS_Port_EnterCritical();
    g_log_level = level;
    MyRTOS_Port_ExitCritical();
}

void Log_Printf(LogLevel_t level, const char* tag, const char* format, ...) {
    if (g_log_queue == NULL || level > g_log_level) {
        return;
    }
    
    char temp_buffer[MYRTOS_LOG_MSG_MAX_SIZE];
    
    int header_len = snprintf(temp_buffer, sizeof(temp_buffer),
        MYRTOS_LOG_FORMAT);
    
    va_list args;
    va_start(args, format);
    int body_len = vsnprintf(temp_buffer + header_len, sizeof(temp_buffer) - header_len, format, args);
    va_end(args);

    int total_len = header_len + body_len;
    if (total_len < (MYRTOS_LOG_MSG_MAX_SIZE - 2)) {
        temp_buffer[total_len] = '\n';
        temp_buffer[total_len + 1] = '\0';
    } else {
        temp_buffer[MYRTOS_LOG_MSG_MAX_SIZE - 2] = '\n';
        temp_buffer[MYRTOS_LOG_MSG_MAX_SIZE - 1] = '\0';
    }

    Queue_Send(g_log_queue, temp_buffer, 0); 
}

#endif // MYRTOS_SERVICE_LOG_ENABLE