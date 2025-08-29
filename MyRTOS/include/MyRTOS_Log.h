#ifndef MYRTOS_LOG_H
#define MYRTOS_LOG_H

#include "MyRTOS_Config.h"

#if (MY_RTOS_USE_LOG == 1)

/**
 * @brief ��־ģ�鶨��
 */
typedef enum {
    LOG_MODULE_SYSTEM,
    LOG_MODULE_USER
} LogModule_t;


// ��ʼ����־ϵͳ (������־����Ͷ���)
void MyRTOS_Log_Init(void);

// --- �û��ӿ� ---
// PRINT �����Ϊ����Ϊ�����κ�ǰ׺��ֱ�����
#if (MY_RTOS_USE_CONSOLE == 1)
#include "MyRTOS_Console.h"
#define PRINT(...) MyRTOS_Console_Printf(__VA_ARGS__)
#else
// ���û�� Console, PRINT ��ʱ�����û���˵�ƽ̨��
#define PRINT(...)
#endif


#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// �ڲ�ʵ�ֺ���
void MyRTOS_Log_Vprintf(LogModule_t module, int level, const char *file, int line, const char *fmt, ...);

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_ERROR)
#define SYS_LOGE(fmt, ...) MyRTOS_Log_Vprintf(LOG_MODULE_SYSTEM, SYS_LOG_LEVEL_ERROR, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define SYS_LOGE(fmt, ...)
#endif

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_WARN)
#define SYS_LOGW(fmt, ...) MyRTOS_Log_Vprintf(LOG_MODULE_SYSTEM, SYS_LOG_LEVEL_WARN, NULL, 0, fmt, ##__VA_ARGS__)
#else
#define SYS_LOGW(fmt, ...)
#endif

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_INFO)
#define SYS_LOGI(fmt, ...) MyRTOS_Log_Vprintf(LOG_MODULE_SYSTEM, SYS_LOG_LEVEL_INFO, NULL, 0, fmt, ##__VA_ARGS__)
#else
#define SYS_LOGI(fmt, ...)
#endif

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_DEBUG)
#define SYS_LOGD(fmt, ...) MyRTOS_Log_Vprintf(LOG_MODULE_SYSTEM, SYS_LOG_LEVEL_DEBUG, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define SYS_LOGD(fmt, ...)
#endif


// --- �û���־�� ---
#define USER_LOGE(fmt, ...) MyRTOS_Log_Vprintf(LOG_MODULE_USER, SYS_LOG_LEVEL_ERROR, NULL, 0, fmt, ##__VA_ARGS__)
#define USER_LOGW(fmt, ...) MyRTOS_Log_Vprintf(LOG_MODULE_USER, SYS_LOG_LEVEL_WARN,  NULL, 0, fmt, ##__VA_ARGS__)
#define USER_LOGI(fmt, ...) MyRTOS_Log_Vprintf(LOG_MODULE_USER, SYS_LOG_LEVEL_INFO,  NULL, 0, fmt, ##__VA_ARGS__)
#define USER_LOGD(fmt, ...) MyRTOS_Log_Vprintf(LOG_MODULE_USER, SYS_LOG_LEVEL_DEBUG, NULL, 0, fmt, ##__VA_ARGS__)


/**
 * @brief ������ʱ����ָ����־ģ��Ĺ��˼���
 * @param module Ҫ���õ�ģ�� (LOG_MODULE_SYSTEM �� LOG_MODULE_USER)
 * @param level  Ҫ���õ������־����
 */
void MyRTOS_Log_SetLevel(LogModule_t module, int level);



#else
// ����������־�ʹ�ӡ
#define MyRTOS_Log_Init()
#define PRINT(...)
#define SYS_LOGE(fmt, ...)
#define SYS_LOGW(fmt, ...)
#define SYS_LOGI(fmt, ...)
#define SYS_LOGD(fmt, ...)
#define USER_LOGE(fmt, ...)
#define USER_LOGW(fmt, ...)
#define USER_LOGI(fmt, ...)
#define USER_LOGD(fmt, ...)
#define MyRTOS_Log_SetLevel(module, level)

#endif

#endif
