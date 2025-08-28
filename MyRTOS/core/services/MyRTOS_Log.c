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

// --- �ڲ�״̬ ---
static QueueHandle_t g_log_queue = NULL;
static TaskHandle_t g_log_task_handle = NULL;
static volatile int g_is_monitor_active = 0; // 0: Normal, 1: Monitor

// --- �ڲ�����ԭ�� ---
static void prvLogWriterTask(void *pv);

static void prvSendToLogQueue(const char *str);


/**
 * @brief ��ʼ����־ϵͳ
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
 * @brief ��־���� (Ψһ��������)
 */
static void prvLogWriterTask(void *pv) {
    char log_buffer[SYS_LOG_MAX_MSG_LENGTH];
    while (1) {
        if (Queue_Receive(g_log_queue, log_buffer, MY_RTOS_MAX_DELAY)) {
            // ������ڼ�����ģʽ���ʹ�ӡ
            if (!g_is_monitor_active) {
                const char *ptr = log_buffer;
                while (*ptr) MyRTOS_Platform_PutChar(*ptr++);
            }
        }
    }
}

/**
 * @brief ����ʽ���õ��ַ������͵����е�ͳһ���
 */
static void prvSendToLogQueue(const char *str) {
    if (g_log_queue) {
        // ʹ��0��ʱ������������˾Ͷ�������������
        Queue_Send(g_log_queue, str, 0);
    }
}

/**
 * @brief PRINT(...) �ĺ��ʵ��
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
 * @brief SYS_LOGx(...) �ĺ��ʵ��
 */
void MyRTOS_Log_Vprintf(int level, const char *file, int line, const char *fmt, ...) {
    if (level > SYS_LOG_LEVEL) return; // ��ȷ����־������

    char temp_buffer[SYS_LOG_MAX_MSG_LENGTH];
    char *p = temp_buffer;
    int len = 0;
    static const char *level_map = " EWID"; // " " for NONE

    // ��ʽ��ǰ׺
    len += snprintf(p, sizeof(temp_buffer), "[%c][%llu] %s:%d: ",
                    level_map[level], MyRTOS_GetTick(), file, line);

    // ��ʽ���û���Ϣ
    va_list args;
    va_start(args, fmt);
    len += vsnprintf(p + len, sizeof(temp_buffer) - len, fmt, args);
    va_end(args);

    // ��ӻ���
    if (len < sizeof(temp_buffer) - 2) {
        p[len++] = '\r';
        p[len++] = '\n';
        p[len] = '\0';
    } else {
        // ȷ����ȫ��β
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


// --- Ϊ�������ṩ�Ŀ��ƽӿ� ---
void MyRTOS_Log_SetMonitorMode(int active) {
    g_is_monitor_active = active;
}

#endif // MY_RTOS_USE_LOG
