#if 0
/**
 *  @brief MyRTOS ��չ����� - ȫ�������ļ�
 */
#ifndef MYRTOS_SERVICE_CONFIG_H
#define MYRTOS_SERVICE_CONFIG_H

/*==================================================================================================
 *                                    ģ�鹦�ܿ���
 *================================================================================================*/

/** @brief ���� IO ������ģ�� (��־��Shellģ��Ļ���) */
#define MYRTOS_SERVICE_IO_ENABLE 1

/** @brief ���������ʱ������ģ�� */
#define MYRTOS_SERVICE_TIMER_ENABLE 1

/** @brief ������־����ģ�� (���� IO ��) */
#define MYRTOS_SERVICE_LOG_ENABLE 1

/** @brief ���� Shell ����ģ�� (���� IO ��) */
#define MYRTOS_SERVICE_SHELL_ENABLE 1

/** @brief ����ϵͳ��ط���ģ�� */
#define MYRTOS_SERVICE_MONITOR_ENABLE 1

/** @brief ���������ն˷���ģ�� */
#define MYRTOS_SERVICE_VTS_ENABLE 1


/*==================================================================================================
 *                                    ģ���������
 *                            (���ڶ�Ӧģ�鿪��ʱ���������ò���Ч)
 *================================================================================================*/

#if MYRTOS_SERVICE_IO_ENABLE == 1
/** @brief Stream_Printf �� Stream_VPrintf ʹ�õ��ڲ���ʽ����������С (�ֽ�) */
#define MYRTOS_IO_PRINTF_BUFFER_SIZE 128
#endif

#if MYRTOS_SERVICE_LOG_ENABLE == 1
#define MYRTOS_LOG_MAX_LEVEL LOG_LEVEL_INFO
#define MYRTOS_LOG_FORMAT                                                                                              \
    "[%c][%s]",                                                                                                        \
            (level == LOG_LEVEL_ERROR ? 'E'                                                                            \
                                      : (level == LOG_LEVEL_WARN ? 'W' : (level == LOG_LEVEL_INFO ? 'I' : 'D'))),      \
            tag
#define MYRTOS_LOG_QUEUE_LENGTH 32
#define MYRTOS_LOG_MSG_MAX_SIZE 128
#define MYRTOS_LOG_TASK_STACK_SIZE 2048
#define MYRTOS_LOG_TASK_PRIORITY 1
#endif

#if MYRTOS_SERVICE_TIMER_ENABLE == 1
/** @brief ��ʱ����������������е���� (�ܻ�����ٸ�����) */
#define MYRTOS_TIMER_COMMAND_QUEUE_SIZE 10
#endif

#if MYRTOS_SERVICE_SHELL_ENABLE == 1
/** @brief Shell ����֧�ֵ����������� (���������) */
#define SHELL_MAX_ARGS 10
/** @brief Shell ���������뻺��������󳤶� (�ֽ�) */
#define SHELL_CMD_BUFFER_SIZE 64
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#define VTS_TASK_PRIORITY 5
#define VTS_TASK_STACK_SIZE 256
#define VTS_RW_BUFFER_SIZE 128
#define VTS_PIPE_BUFFER_SIZE 512
#define VTS_MAX_BACK_CMD_LEN 16 // "back"�������е���󳤶�
#define VTS_RW_BUFFER_SIZE 128 // �ڲ���д��������С
#endif


/*==================================================================================================
 *                                      ������ϵ���
 *                                (��ֹ��������������)
 *================================================================================================*/

#if defined(MYRTOS_SERVICE_LOG_ENABLE) && !defined(MYRTOS_SERVICE_IO_ENABLE)
#error "���ô���: ��־ģ�� (MYRTOS_LOG_ENABLE) ������ IO��ģ�� (MYRTOS_IO_ENABLE)!"
#endif

#if defined(MYRTOS_SERVICE_SHELL_ENABLE) && !defined(MYRTOS_SERVICE_IO_ENABLE)
#error "���ô���: Shellģ�� (MYRTOS_SHELL_ENABLE) ������ IO��ģ�� (MYRTOS_IO_ENABLE)!"
#endif

#if defined(MYRTOS_SERVICE_VTS_ENABLE) && !defined(MYRTOS_SERVICE_IO_ENABLE)
#error "���ô���: �����ն�ģ�� (MYRTOS_VTS_ENABLE) ������ IO��ģ�� (MYRTOS_IO_ENABLE)!"
#endif

/*==================================================================================================
 *                                      �պ궨��
 *                  (���ģ�鱻����, ����պ��Ա�֤�ϲ���������޸ļ��ɱ���)
 *================================================================================================*/
// --- ��־ģ�� ---
#if MYRTOS_SERVICE_LOG_ENABLE == 0
#define LOG_E(tag, format, ...) ((void) 0)
#define LOG_W(tag, format, ...) ((void) 0)
#define LOG_I(tag, format, ...) ((void) 0)
#define LOG_D(tag, format, ...) ((void) 0)
#define LOG_V(tag, format, ...) ((void) 0)
#define Log_Init(initial_level, default_stream) (0)
#define Log_SetLevel(level) ((void) 0)
#define Log_SetStream(stream) ((void) 0)
#define Log_Output(level, tag, format, ...) ((void) 0)
#define Log_VOutput(level, tag, format, args) ((void) 0)
#endif

// --- ��ʱ��ģ�� ---
#if MYRTOS_SERVICE_TIMER_ENABLE == 0
#define TimerHandle_t void *
#define TimerService_Init(prio, stack) (0)
#define Timer_Create(name, period, is_p, cb, arg) ((void *) 0)
#define Timer_Start(timer, ticks) (-1)
#define Timer_Stop(timer, ticks) (-1)
#define Timer_Delete(timer, ticks) (-1)
#define Timer_ChangePeriod(timer, period, ticks) (-1)
#define Timer_GetArg(timer) ((void *) 0)
#endif

// --- Shell ģ�� ---
#if MYRTOS_SERVICE_SHELL_ENABLE == 0
#define Shell_Init(config, commands, count) ((void *) 0)
#define Shell_Start(shell_h, name, prio, stack) (-1)
#define Shell_GetStream(shell_h) ((void *) 0)
#endif

// --- ���ģ�� ---
#if MYRTOS_SERVICE_MONITOR_ENABLE == 0
#define Monitor_Init(config) (-1)
#define Monitor_GetNextTask(prev_h) ((void *) 0)
#define Monitor_GetTaskInfo(task_h, stats) (-1)
#define Monitor_GetHeapStats(stats) ((void) 0)
#endif

#endif // MYRTOS_SERVICE_CONFIG_H

#endif
