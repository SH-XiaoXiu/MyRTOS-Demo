#if 0
#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

//======================================================================
//                    PLATFORM & ARCHITECTURE
//======================================================================
// Ŀ��Ӳ��ƽ̨�ĵײ�ͷ�ļ�
#include "gd32f4xx.h"

// ƽ̨��صĺ꣬�����ٽ��������������л���ǿ�Ƶ���
// criticalNestingCount ����֧��Ƕ�׵��ٽ���
extern volatile uint32_t criticalNestingCount;

#define MyRTOS_Port_ENTER_CRITICAL() \
    do { \
        __disable_irq(); \
        criticalNestingCount++; \
    } while(0)

#define MyRTOS_Port_EXIT_CRITICAL() \
    do { \
        if (criticalNestingCount > 0) { \
            criticalNestingCount--; \
            if (criticalNestingCount == 0) { \
                __enable_irq(); \
            } \
        } \
    } while(0)

#define MyRTOS_Port_YIELD()                      do { SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; __ISB(); } while(0)


//======================================================================
//                       CORE KERNEL
//======================================================================
// RTOS�ں˵Ļ������ã�ϵͳ���еı�����
#define MY_RTOS_MAX_PRIORITIES              (16)      // ���֧�ֵ����ȼ�����
#define MY_RTOS_MAX_CONCURRENT_TASKS        (32)      // ��󲢷�����������ǰʵ���²��ܳ���64
#define MY_RTOS_TICK_RATE_HZ                (1000)    // ϵͳTickƵ�� (Hz)
#define MY_RTOS_MAX_DELAY                   (0xFFFFFFFFU) // ����API�������ʱticks
#define MY_RTOS_TASK_NAME_MAX_LEN           (16)      // ���������ַ�������󳤶�

//======================================================================
//                    MEMORY MANAGEMENT
//======================================================================
#define RTOS_MEMORY_POOL_SIZE               (64 * 1024) // �ں˹���ľ�̬�ڴ���ܴ�С (bytes)
#define HEAP_BYTE_ALIGNMENT                 (8)         // ���ڴ����Ķ����ֽ���

//======================================================================
//                  OPTIONAL KERNEL MODULES
//======================================================================
// �ں��еĿ�ѡ����ģ�飬�������Բü�ϵͳ��С
#define MY_RTOS_USE_SOFTWARE_TIMERS         1 // 1: ���������ʱ������; 0: ����
#define MY_RTOS_GENERATE_RUN_TIME_STATS     1 // 1: ��������ʱͳ�ƹ���; 0: ����

//======================================================================
//                      SYSTEM SERVICES & COMPONENTS
//======================================================================
// �������ں�֮�ϵĸ߼��������׼������ɰ�������
#define MY_RTOS_USE_STDIO                   1 // 1: ���ñ�׼I/O�����ǽ���ʽ����Ļ���
#define MY_RTOS_USE_LOG                     1 // 1: ������־����
#define MY_RTOS_USE_SHELL                   1 // 1: ����Shell(�ն�)����
#define MY_RTOS_USE_MONITOR                 1 // 1: ���ñ�׼��ϵͳ���������

//======================================================================
//                 SERVICE-SPECIFIC CONFIGURATIONS
//======================================================================

#if (MY_RTOS_USE_LOG == 1)
// --- ��־������ ---
#define SYS_LOG_LEVEL_NONE              0
#define SYS_LOG_LEVEL_ERROR             1
#define SYS_LOG_LEVEL_WARN              2
#define SYS_LOG_LEVEL_INFO              3
#define SYS_LOG_LEVEL_DEBUG             4
// ϵͳ��־���û���־��Ĭ�Ϲ��˼���
#define SYS_LOG_LEVEL                   SYS_LOG_LEVEL_ERROR
#define USER_LOG_LEVEL                  SYS_LOG_LEVEL_INFO
// --- �첽��־ϵͳ���� ---
#define SYS_LOG_QUEUE_LENGTH            30   // ��־��Ϣ���еĳ���
#define SYS_LOG_MAX_MSG_LENGTH          128  // ������־��Ϣ����󳤶�
#define SYS_LOG_TASK_PRIORITY           1    // ��־��������ȼ�
#define SYS_LOG_TASK_STACK_SIZE         512  // ��־�����ջ��С (words)
#else
#define SYS_LOG_LEVEL_NONE              0
#define SYS_LOG_LEVEL_ERROR             1
#define SYS_LOG_LEVEL_WARN              2
#define SYS_LOG_LEVEL_INFO              3
#define SYS_LOG_LEVEL_DEBUG             4
#define SYS_LOG_LEVEL                   SYS_LOG_LEVEL_DEBUG
#define USER_LOG_LEVEL                  SYS_LOG_LEVEL_DEBUG
#endif

#if (MY_RTOS_USE_MONITOR == 1)
#define SYS_MONITOR_TASK_PRIORITY       (1)    // ����������ͨ�����ȼ��ϵ�
#define SYS_MONITOR_TASK_STACK_SIZE     (1024) // ��Ҫ�ϴ�ջ�������ַ�����ʽ��
#define SYS_MONITOR_REFRESH_PERIOD_MS   (500) // ˢ������ (ms)
#define MAX_TASKS_FOR_STATS             (MY_RTOS_TASK_NAME_MAX_LEN)
#endif

#if (MY_RTOS_USE_SHELL == 1)
#define SYS_SHELL_TASK_PRIORITY         (2)    // Shell��������ȼ�
#define SYS_SHELL_TASK_STACK_SIZE       (1024) // Shell�����ջ��С (words)
#define SYS_SHELL_MAX_CMD_LENGTH        (64)   // Shell�����������󳤶�
#define SYS_SHELL_MAX_ARGS              (8)    // Shell��������������������
#define SYS_SHELL_PROMPT                "MyRTOS> " // Shell����������ʾ��
#endif

//======================================================================
//                      HARDWARE DRIVER MAPPING
//======================================================================
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
// ������Ŀ��ʹ�õ�Ӳ����ʱ���б�
// ��ʽ: USE_TIMER(�߼�ID, ����ʱ��, Ԥ������)
#define MY_RTOS_TIMER_DEVICE_LIST \
        USE_TIMER(PERF_COUNTER_TIMER, TIMER1, 0) \
        USE_TIMER(USER_APP_TIMER,     TIMER2, 0)

// ������ʱͳ�ƹ��ܰ󶨵�һ�������Ӳ����ʱ���߼�ID
#define MY_RTOS_STATS_TIMER_ID          TIMER_ID_PERF_COUNTER_TIMER
// �����ö�ʱ���ļ���Ƶ�� (Hz)���������ݴ˼���Ԥ��Ƶ
#define MY_RTOS_STATS_TIMER_FREQ_HZ     1000000
#endif

//======================================================================
//                     CONFIGURATION DEPENDENCY CHECKS
//======================================================================
// ����ʱ��飬��ֹ���Ϸ����������
#if (MY_RTOS_USE_LOG == 1 && MY_RTOS_USE_STDIO == 0)
#error "Log Service requires StdIO Service. Please enable MY_RTOS_USE_STDIO."
#endif

#if (MY_RTOS_USE_SHELL == 1 && MY_RTOS_USE_STDIO == 0)
#error "Shell Service requires StdIO Service. Please enable MY_RTOS_USE_STDIO."
#endif

#if (MY_RTOS_USE_MONITOR == 1 && MY_RTOS_USE_STDIO == 0)
#error "Default Monitor Component implementation requires StdIO Service. Please enable MY_RTOS_USE_STDIO."
#endif

#if (MY_RTOS_USE_MONITOR == 1 && MY_RTOS_GENERATE_RUN_TIME_STATS == 0)
#error "Monitor Component requires Run-Time Stats. Please enable MY_RTOS_GENERATE_RUN_TIME_STATS."
#endif


#endif // MYRTOS_CONFIG_H

#endif
