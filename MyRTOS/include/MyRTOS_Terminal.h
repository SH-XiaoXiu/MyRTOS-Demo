#ifndef MYRTOS_TERMINAL_H
#define MYRTOS_TERMINAL_H

#include "MyRTOS_Config.h"

#if (MY_RTOS_USE_TERMINAL == 1)

/**
 * @brief ��������ĺ���ָ�����͡�
 * @param argc ��������
 * @param argv �����ַ�������
 */
typedef void (*TerminalCommandFunc_t)(int argc, char **argv);

/**
 * @brief ��ʼ�� Terminal ��������Ļ�����ʩ��
 *        �˺����������� Terminal ����
 *        Ӧ���� MyRTOS_SystemInit �б����á�
 */
void MyRTOS_Terminal_Init(void);

/**
 * @brief ���� Terminal ����
 *        ������ Terminal ����׼���������롣
 * @return 0 �ɹ�, -1 ʧ�ܡ�
 */
int MyRTOS_Terminal_Start(void);

/**
 * @brief ֹͣ Terminal ����
 *        ������ Terminal ����
 * @return 0 �ɹ�, -1 ʧ�ܡ�
 */
int MyRTOS_Terminal_Stop(void);

/**
 * @brief ��ѯ Terminal �Ƿ��������С�
 * @return 1 ��������, 0 ��ֹͣ��
 */
int MyRTOS_Terminal_IsRunning(void);

/**
 * @brief ���ն˷���ע��һ�������
 *        �˺������̰߳�ȫ�ġ�
 * @param name ��������� (e.g., "ps")
 * @param help ����İ���˵��
 * @param func ����Ĵ�����
 * @return 0 �ɹ�, -1 ʧ�� (���ڴ治��������Ѵ���)
 */
int MyRTOS_Terminal_RegisterCommand(const char* name, const char* help, TerminalCommandFunc_t func);

#else
// �������, �ṩ�պ�
#define MyRTOS_Terminal_Init()
#define MyRTOS_Terminal_Start() (0)
#define MyRTOS_Terminal_Stop() (0)
#define MyRTOS_Terminal_IsRunning() (0)
#define MyRTOS_Terminal_RegisterCommand(name, help, func) (0)
#endif // MY_RTOS_USE_TERMINAL

#endif // MYRTOS_TERMINAL_H