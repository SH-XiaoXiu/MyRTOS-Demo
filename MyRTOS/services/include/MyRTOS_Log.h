#ifndef MYRTOS_LOG_H
#define MYRTOS_LOG_H

#include "MyRTOS_Service_Config.h"
#include "MyRTOS_IO.h"

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)

// ��־�ȼ�ö��
typedef enum {
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
} LogLevel_t;

// --- ���� API ---
int Log_Init(LogLevel_t initial_level, StreamHandle_t output_stream);

void Log_SetLevel(LogLevel_t level);

#ifndef MYRTOS_LOG_MAX_LEVEL
#define MYRTOS_LOG_MAX_LEVEL LOG_LEVEL_DEBUG
#endif

#ifndef MYRTOS_LOG_FORMAT
#define MYRTOS_LOG_FORMAT "[%5llu][%c][%s]"
#endif


// --- ��־��ӡ�� (��Ӧ�ÿ�����ʹ�õı�ݽӿ�) ---
#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_ERROR)
#define LOG_E(tag, format, ...) Log_Printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#else
#define LOG_E(tag, format, ...) do {} while(0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_WARN)
#define LOG_W(tag, format, ...) Log_Printf(LOG_LEVEL_WARN,  tag, format, ##__VA_ARGS__)
#else
#define LOG_W(tag, format, ...) do {} while(0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_INFO)
#define LOG_I(tag, format, ...) Log_Printf(LOG_LEVEL_INFO,  tag, format, ##__VA_ARGS__)
#else
#define LOG_I(tag, format, ...) do {} while(0)
#endif

#if (MYRTOS_LOG_MAX_LEVEL >= LOG_LEVEL_DEBUG)
#define LOG_D(tag, format, ...) Log_Printf(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#else
#define LOG_D(tag, format, ...) do {} while(0)
#endif

// --- �ײ��ӡ���� (�ɺ����) ---
void Log_Printf(LogLevel_t level, const char *tag, const char *format, ...);

#else // ������񱻽���
#define Log_Init(level, stream) (0)
#define Log_SetLevel(level)
#define LOG_E(tag, format, ...)
#define LOG_W(tag, format, ...)
#define LOG_I(tag, format, ...)
#define LOG_D(tag, format, ...)
#endif // MYRTOS_SERVICE_LOG_ENABLE

#endif // MYRTOS_LOG_H
