#if 0
#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H


// ƽ̨����Զ�����MyRTOS�������ļ����Ծ�����Ҫ��ʼ����Щ����
// ��ȷ�������Ŀ����·����ȷ���ã������ҵ��������ļ���
#include "MyRTOS_Config.h"
#include "MyRTOS_Service_Config.h"

// =========================================================================
//                   ��ִ�г��� ����
// =========================================================================
#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
#define PLATFORM_PROGRAM_LAUNCH_STACK            64 // ��������ջ��С
#endif


// =========================================================================
//                   ���Կ���̨ (Console) ����
// =========================================================================
#define PLATFORM_USE_CONSOLE             1 // �Զ�ʹ��ƽ̨����̨

//ѡ��ʹ�õ�USART���� ��Ϊȫ�ֱ�׼IO
#define PLATFORM_CONSOLE_USART_NUM       0
//������
#define PLATFORM_CONSOLE_BAUDRATE        115200U

//�жϺͻ����� ---
#define PLATFORM_CONSOLE_RX_BUFFER_SIZE  128
#define PLATFORM_CONSOLE_IRQ_PRIORITY    5


// =========================================================================
//                   �߾��ȶ�ʱ�� (Monitor Service) ����
// =========================================================================
#if (MYRTOS_SERVICE_MONITOR_ENABLE == 1)
#define PLATFORM_USE_HIRES_TIMER         1 // �Զ�ʹ�ܸ߾��ȶ�ʱ��

//ѡ�������߾���ʱ����ͨ�ö�ʱ�� ---
// �Ƽ�ʹ��32λ��ʱ������ TIMER1, TIMER2, TIMER3, TIMER4 (��GD32�ж�Ӧ��Ϊ1,2,3,4)
#define PLATFORM_HIRES_TIMER_NUM         1

//Ƶ�� (Hz) ---
// 1MHz (1 tick = 1us) ��һ���ܺõ�ѡ�񣬱��ڵ���
#define PLATFORM_HIRES_TIMER_FREQ_HZ     1000000U
#else
#define PLATFORM_USE_HIRES_TIMER         0
#endif

// =========================================================================
//                   ƽ̨ Hook  ����
// =========================================================================
#define PLATFORM_USE_ERROR_HOOK             1

// =========================================================================
//                   ƽ̨ Shell  ����
// =========================================================================
#if MYRTOS_SERVICE_SHELL_ENABLE == 1
#define PLATFORM_USE_DEFAULT_COMMANDS 1
#define PLATFORM_USE_PROGRAM_MANGE 1
#define MAX_REGISTERED_PROGRAMS 16
#else
#define PLATFORM_USE_DEFAULT_COMMANDS 0
#define PLATFORM_USE_PROGRAM_MANGE 0
#endif


#endif // PLATFORM_CONFIG_H


#endif
