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
 * @brief ��ʼ����־ϵͳ
 */
void MyRTOS_Log_Init(void) {
    if (g_log_queue) return;

    g_log_queue = Queue_Create(SYS_LOG_QUEUE_LENGTH, SYS_LOG_MAX_MSG_LENGTH);
    if (!g_log_queue) {
        // ������׶Σ�StdIO��δ��ȫ������ʹ����ԭʼ�ķ�ʽ��ӡ
        const char *err_msg = "Error: Failed to create log queue.\n";
        for (const char *p = err_msg; *p; ++p) MyRTOS_Platform_PutChar(*p);
        return;
    }

    g_log_task_handle = Task_Create(prvLogWriterTask, "LogWriter", SYS_LOG_TASK_STACK_SIZE, NULL,
                                    SYS_LOG_TASK_PRIORITY);
    if (!g_log_task_handle) {
        // �������񴴽�ʧ��
        const char *err_msg = "Error: Failed to create log task.\n";
        for (const char *p = err_msg; *p; ++p) MyRTOS_Platform_PutChar(*p);
    }
    // ����־���Ӻ���ע�ᵽ�ں�
    MyRTOS_RegisterKernelLogHook(prvKernelLogHook);
}

/**
 * @brief ��־���� (Ψһ��������)
 *        ������־������ȡ����Ϣ���������ӡ����׼��������
 */
static void prvLogWriterTask(void *pv) {
    char log_buffer[SYS_LOG_MAX_MSG_LENGTH];
    while (1) {
        // �������������ȴ��µ���־��Ϣ
        if (Queue_Receive(g_log_queue, log_buffer, MY_RTOS_MAX_DELAY)) {
            // ����־�������׼������
            //������Խ���־�볣���������
            MyRTOS_fprintf(g_myrtos_std_err, "%s", log_buffer);
        }
    }
}

/**
 * @brief ����ʽ���õ��ַ������͵����е�ͳһ���
 */
static void prvSendToLogQueue(const char *str, int len) {
    if (g_log_queue && len < SYS_LOG_MAX_MSG_LENGTH) {
        // ʹ��0��ʱ������������˾Ͷ�����־��������������������
        Queue_Send(g_log_queue, str, 0);
    }
}

/**
 * @brief ����ע����ں˵Ĺ��Ӻ��������ڴ����ں���־��
 */
static void prvKernelLogHook(const char *message, uint16_t length) {
    // �ں���־����Ϊϵͳ��DEBUG����
    if (g_system_log_level < SYS_LOG_LEVEL_DEBUG) {
        return;
    }
    char temp_buffer[SYS_LOG_MAX_MSG_LENGTH];
    // ��ʽ���ں���־��Ϊ�����ǰ׺
    int len = snprintf(temp_buffer, sizeof(temp_buffer), "[S][D][KERNEL] %.*s\n", length, message);
    if (len > 0) {
        // ͬ������ʽ������ں���־���͵�����
        prvSendToLogQueue(temp_buffer, len);
    }
}


/**
 * @brief SYS_LOGx(...) �� USER_LOGx(...) �ĺ��ʵ��
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

    // ��ʽ����־ǰ׺
    len += snprintf(temp_buffer + len, sizeof(temp_buffer) - len, "[%c][%c][%llu] ",
                    module_char, level_map[level], MyRTOS_GetTick());

    // ����ṩ���ļ������кţ��������
    if (file != NULL && line > 0) {
        len += snprintf(temp_buffer + len, sizeof(temp_buffer) - len, "%s:%d: ", file, line);
    }

    // ��ʽ���û���Ϣ
    va_list args;
    va_start(args, fmt);
    len += vsnprintf(temp_buffer + len, sizeof(temp_buffer) - len, fmt, args);
    va_end(args);

    // ȷ���Ի��з���β
    if (len < (int) sizeof(temp_buffer) - 1) {
        temp_buffer[len++] = '\n';
        temp_buffer[len] = '\0';
    } else {
        // ���������ˣ�ǿ����ĩβ��ӻ��з�
        temp_buffer[sizeof(temp_buffer) - 2] = '\n';
        temp_buffer[sizeof(temp_buffer) - 1] = '\0';
        len = sizeof(temp_buffer);
    }

    // �ؼ��߼����ж�������
    if (!MyRTOS_Schedule_IsRunning() || g_log_queue == NULL) {
        // ������δ���л���д���ʧ�ܣ�ֱ��������
        // MyRTOS_fprintf �ڲ����Զ������������
        MyRTOS_fprintf(NULL, "%s", temp_buffer);
    } else {
        // ������������͵���־����
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
