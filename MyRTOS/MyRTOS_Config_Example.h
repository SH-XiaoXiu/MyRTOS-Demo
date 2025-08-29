#if 0
#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

//====================== ƽ̨��ܹ����� ======================
//����Ŀ��Ӳ��ƽ̨�ĵײ�ͷ�ļ�
#include "gd32f4xx.h"

extern volatile uint32_t criticalNestingCount;
//����ƽ̨��صĺ� (�ٽ���, Yield��)
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


//====================== �ں˺������� ======================
#define MY_RTOS_MAX_PRIORITIES              (16)      // ���֧�ֵ����ȼ�����
#define MY_RTOS_TICK_RATE_HZ                (1000)    // ϵͳTickƵ�� (Hz)
#define MY_RTOS_MAX_DELAY                   (0xFFFFFFFFU) // �����ʱticks
#define MY_RTOS_TASK_NAME_MAX_LEN           (16)       // ����������󳤶�

//====================== �ڴ�������� ======================
#define RTOS_MEMORY_POOL_SIZE               (32 * 1024) // �ں�ʹ�õľ�̬�ڴ�ش�С (bytes)
#define HEAP_BYTE_ALIGNMENT                 (8)         // �ڴ�����ֽ���

//====================== ��־����������� ======================
#define MY_RTOS_USE_LOG                     1 // 1: ������־����; 0: ����
#define MY_RTOS_USE_MONITOR                 1 // 1: ����ϵͳ������ģ��; 0: ����
#if MY_RTOS_USE_MONITOR
#define MY_RTOS_MONITOR_KERNEL_LOG          1 //1: �����ں���־; 0: �����ں���־
#endif


#if (MY_RTOS_USE_LOG == 1)
// --- ��־������ ---
#define SYS_LOG_LEVEL_NONE              0 // ����ӡ�κ���־
#define SYS_LOG_LEVEL_ERROR             1 // ֻ��ӡ����
#define SYS_LOG_LEVEL_WARN              2 // ��ӡ����;���
#define SYS_LOG_LEVEL_INFO              3 // ��ӡ���󡢾������Ϣ
#define SYS_LOG_LEVEL_DEBUG             4 // ��ӡ���м���

// ���õ�ǰϵͳ����־����
#define SYS_LOG_LEVEL                   SYS_LOG_LEVEL_INFO

// --- �첽��־ϵͳ���� ---
#define SYS_LOG_QUEUE_LENGTH            30
#define SYS_LOG_MAX_MSG_LENGTH          128
#define SYS_LOG_TASK_PRIORITY           1
#define SYS_LOG_TASK_STACK_SIZE         512
#endif

#if (MY_RTOS_USE_MONITOR == 1)
// ������������������
#define MY_RTOS_MONITOR_TASK_PRIORITY      (1)
#define MY_RTOS_MONITOR_TASK_STACK_SIZE    (1024) // ��Ҫ�ϴ�ջ�����ɻ�����
#define MY_RTOS_MONITOR_TASK_PERIOD_MS     (500) // ������ˢ������
#define MY_RTOS_MONITOR_BUFFER_SIZE        (2048)
#define MAX_TASKS_FOR_STATS                (32) //������������
#endif

//====================== ����ʱͳ������ ======================
// 1: ��������ʱͳ�ƹ���; 0: �رա����Ǽ������Ļ�����
#define MY_RTOS_GENERATE_RUN_TIME_STATS     1

//====================== Ӳ���������� ======================

// --- ��ʱ���������� ---
// ������Ҫʹ�õĶ�ʱ�����߼�ʵ����
// ��ʽ��USE_TIMER(�߼�ID, ����ʱ��, Ԥ������)
#define MY_RTOS_TIMER_DEVICE_LIST \
    USE_TIMER(PERF_COUNTER_TIMER, TIMER1, 0) \
    USE_TIMER(USER_APP_TIMER,     TIMER2, 0)

// --- ģ��������ӳ�� ---
// Ϊ����ʱͳ�ƹ��ܷ���һ���߾���ʱ��Դ
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
#define MY_RTOS_STATS_TIMER_ID          TIMER_ID_PERF_COUNTER_TIMER
#define MY_RTOS_STATS_TIMER_FREQ_HZ     1000000 // ������Ƶ�� (1MHz)
#endif

#endif // MYRTOS_CONFIG_H

#endif
