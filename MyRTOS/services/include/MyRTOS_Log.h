/**
 * @file  MyRTOS_Log.h
 * @brief MyRTOS ��־�������첽IO
 */
#ifndef MYRTOS_LOG_H
#define MYRTOS_LOG_H

#include <stdarg.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Service_Config.h"

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)

// ��־�ȼ�ö��
typedef enum {
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
} LogLevel_t;

#ifndef MYRTOS_LOG_MSG_MAX_SIZE
#define MYRTOS_LOG_MSG_MAX_SIZE 128
#endif


// �첽IO����ṹ��
typedef struct {
    StreamHandle_t target_stream; // Ŀ����
    char message[MYRTOS_LOG_MSG_MAX_SIZE]; // ��Ϣ����
} AsyncWriteRequest_t;


// ���� API

/**
 * @brief ��ʼ���첽��־/IO����
 * @details �ú����ᴴ��һ�������ȼ��ĺ�̨�������������еĴ�ӡ����
 *          ȷ�����ô�ӡ������ҵ�����񲻻ᱻIO������
 * @return int 0 ��ʾ�ɹ�, -1 ��ʾʧ�ܡ�
 */
int Log_Init(void);

/**
 * @brief ����ȫ����־���˵ȼ���
 * @param level [in] �µ���־�ȼ������ڴ˵ȼ�����־�������ԡ�
 */
void Log_SetLevel(LogLevel_t level);

// ��־��ӡ�� (��Ӧ�ÿ�����ʹ�õı�ݽӿ�)

#ifndef MYRTOS_LOG_MAX_LEVEL
#define MYRTOS_LOG_MAX_LEVEL LOG_LEVEL_DEBUG
#endif

// ��־�����ڵ��� Log_Printf�����ڲ���ʹ���첽����
#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_ERROR)
#define LOG_E(tag, format, ...) Log_Printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#else
#define LOG_E(tag, format, ...)                                                                                        \
    do {                                                                                                               \
    } while (0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_WARN)
#define LOG_W(tag, format, ...) Log_Printf(LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#else
#define LOG_W(tag, format, ...)                                                                                        \
    do {                                                                                                               \
    } while (0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_INFO)
#define LOG_I(tag, format, ...) Log_Printf(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#else
#define LOG_I(tag, format, ...)                                                                                        \
    do {                                                                                                               \
    } while (0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_DEBUG)
#define LOG_D(tag, format, ...) Log_Printf(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#else
#define LOG_D(tag, format, ...)                                                                                        \
    do {                                                                                                               \
    } while (0)
#endif

// �ײ��ӡ����

/**
 * @brief (�ײ㺯��) ����־����ã�����ʽ������־��Ϣ��ΪIO�����͵����С�
 */
void Log_Printf(LogLevel_t level, const char *tag, const char *format, ...);

/**
 * @brief (�ײ㺯��) �첽����ָ������ӡ��ʽ���ַ��� (va_list�汾)��
 */
void MyRTOS_AsyncVprintf(StreamHandle_t stream, const char *format, va_list args);

/**
 * @brief �첽����ָ������ӡ��ʽ���ַ�����
 */
void MyRTOS_AsyncPrintf(StreamHandle_t stream, const char *format, ...);

// MyRTOS_printf ����Ӧ��Ҳ����첽�ģ��Ա���ϵͳ��Ϊһ��
#undef MyRTOS_printf
#define MyRTOS_printf(format, ...) MyRTOS_AsyncPrintf(Task_GetStdOut(NULL), format, ##__VA_ARGS__)

#else // ������񱻽���
#define Log_Init() (0)
#define Log_SetLevel(level)
#define LOG_E(tag, format, ...)
#define LOG_W(tag, format, ...)
#define LOG_I(tag, format, ...)
#define LOG_D(tag, format, ...)
#define MyRTOS_printf(format, ...) // �����־������ã�printfҲӦ�����û��ض���ͬ���汾
#endif // MYRTOS_SERVICE_LOG_ENABLE

#endif // MYRTOS_LOG_H
