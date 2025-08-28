//
// Created by XiaoXiu on 8/28/2025.
//

#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

//====================== �ں˺������� ======================
#define MY_RTOS_MAX_PRIORITIES              (16)      // ���֧�ֵ����ȼ�����
#define MY_RTOS_TICK_RATE_HZ                (1000)    // ϵͳTickƵ�� (Hz)
#define MY_RTOS_MAX_DELAY                   (0xFFFFFFFFU) // �����ʱticks
#define MY_RTOS_TASK_NAME_MAX_LEN           (16)       // ����������󳤶�

//====================== �ڴ�������� ======================
#define RTOS_MEMORY_POOL_SIZE               (32 * 1024) // �ں�ʹ�õľ�̬�ڴ�ش�С (bytes)
#define HEAP_BYTE_ALIGNMENT                 (8)         // �ڴ�����ֽ���

//====================== ϵͳ�������� ======================

// --- ��׼I/O (printf) ���� ---
#define MY_RTOS_USE_STDIO                   1 // 1: ����printf�ض������; 0: ����
#if (MY_RTOS_USE_STDIO == 1)
// �������ڱ���I/O����Ļ������ȴ�ʱ��
#define MY_RTOS_IO_MUTEX_TIMEOUT_MS     100
#endif

//====================== ��־��I/O���� ======================
#define MY_RTOS_USE_LOG                     1 // 1: ������־��printf����; 0: ����
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


//====================== ����ʱͳ������������� ======================

// --- ����ʱͳ�ƹ��� ---
// 1: ��������ʱͳ�ƹ���; 0: �رա����Ǽ������Ļ��������뿪����
#define MY_RTOS_GENERATE_RUN_TIME_STATS     1

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
// --- ϵͳ������ (Monitor) ---
// 1: ����ϵͳ������ģ��; 0: ����
#define MY_RTOS_USE_MONITOR             1

#if (MY_RTOS_USE_MONITOR == 1)
// ������������������
#define MY_RTOS_MONITOR_TASK_PRIORITY      (1)
#define MY_RTOS_MONITOR_TASK_STACK_SIZE    (1024) // ��Ҫ�ϴ�ջ�����ɻ�����
#define MY_RTOS_MONITOR_TASK_PERIOD_MS     (1000) // ������ˢ������

// ���������ڸ�ʽ������Ļ�������С
#define MY_RTOS_MONITOR_BUFFER_SIZE        (2048)
#endif
#endif


#endif // MYRTOS_CONFIG_H
