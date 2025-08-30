#ifndef MYRTOS_SHELL_H
#define MYRTOS_SHELL_H

#include "MyRTOS_Config.h"

#if (MY_RTOS_USE_SHELL == 1)

#include "MyRTOS.h" // For TaskHandle_t

/**
 * @brief Shell��������ĺ���ָ�����͡�
 * @param argc ��������
 * @param argv �����ַ�������
 */
typedef void (*ShellCommandFunc_t)(int argc, char **argv);

/**
 * @brief ��ʼ�� Shell ����
 *        ��Ҫ��ע���������������������
 *        Ӧ�� MyRTOS_SystemInit �б����á�
 */
void MyRTOS_Shell_Init(void);

/**
 * @brief ���� Shell ����
 *        �˺����ᴴ��������Shell���������Shell�������У������κ��¡�
 * @return 0 �ɹ�, -1 ʧ�� (�����񴴽�ʧ��)��
 */
int MyRTOS_Shell_Start(void);

/**
 * @brief ֹͣ Shell ����
 *        �˺����������������е�Shell�������Shellδ���У������κ��¡�
 * @return 0 �ɹ���
 */
int MyRTOS_Shell_Stop(void);

/**
 * @brief ��ѯ Shell �����Ƿ��������С�
 * @return 1 ��������, 0 ��ֹͣ��
 */
int MyRTOS_Shell_IsRunning(void);

/**
 * @brief �� Shell ����ע��һ�������
 * @param name ��������� (e.g., "ps")
 * @param help ����İ���˵��
 * @param func ����Ĵ�����
 * @return 0 �ɹ�, -1 ʧ�� (���ڴ治��)
 */
int MyRTOS_Shell_RegisterCommand(const char* name, const char* help, ShellCommandFunc_t func);

#else
#define MyRTOS_Shell_Init()
#define MyRTOS_Shell_Start()      (0)
#define MyRTOS_Shell_Stop()       (0)
#define MyRTOS_Shell_IsRunning()  (0)
#define MyRTOS_Shell_RegisterCommand(name, help, func) (0)
#endif // MY_RTOS_USE_SHELL

#endif // MYRTOS_SHELL_H