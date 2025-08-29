//
// Created by XiaoXiu on 8/29/2025.
//

#ifndef MYRTOS_CONSOLE_H
#define MYRTOS_CONSOLE_H

#include "MyRTOS_Config.h"
#include "MyRTOS.h"

#if (MY_RTOS_USE_CONSOLE == 1)

/**
 * @brief ����̨������ģʽ����
 */
typedef enum {
    CONSOLE_MODE_LOG, // Ĭ��ģʽ, ������ʾʵʱ��־
    CONSOLE_MODE_MONITOR, // ������ģʽ, ȫ��ˢ��ϵͳ״̬
    CONSOLE_MODE_TERMINAL // �ն�ģʽ, ���������н���
} ConsoleMode_t;


/**
 * @brief ��ʼ�� Console ����
 *        �˺��������� Console ���������Ķ��С�
 *        Ӧ���� MyRTOS_SystemInit �б����á�
 */
void MyRTOS_Console_Init(void);

/**
 * @brief ͳһ�ġ��̰߳�ȫ�ĸ�ʽ�����������
 *        ������Ҫ�����̨��ӡ��Ϣ���ϲ����(Log, Monitor, Terminal)
 *        ��Ӧ���ô˺���, ������ֱ�ӷ��ʵײ�Ӳ����
 * @param fmt ��ʽ���ַ���
 * @param ... �ɱ����
 */
void MyRTOS_Console_Printf(const char *fmt, ...);

/**
 * @brief ���ÿ���̨�ĵ�ǰ����ģʽ��
 *        �⽫�����ĸ�����ӵ����Ļ���������Ȩ��
 * @param mode Ҫ���õ���ģʽ
 */
void MyRTOS_Console_SetMode(ConsoleMode_t mode);

/**
 * @brief ��ȡ����̨�ĵ�ǰ����ģʽ��
 * @return ��ǰ�� ConsoleMode_t
 */
ConsoleMode_t MyRTOS_Console_GetMode(void);


void MyRTOS_Console_RegisterInputConsumer(QueueHandle_t input_queue);


#else
// ��������� Console, ���ṩ��ʵ���Ա���������
#define MyRTOS_Console_Init()
#define MyRTOS_Console_Printf(fmt, ...)
#define MyRTOS_Console_SetMode(mode)
#define MyRTOS_Console_GetMode() CONSOLE_MODE_LOG

#endif // MY_RTOS_USE_CONSOLE

#endif // MYRTOS_CONSOLE_H
