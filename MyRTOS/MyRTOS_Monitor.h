#ifndef MYRTOS_MONITOR_H
#define MYRTOS_MONITOR_H

#include "MyRTOS_Config.h"

#if (MY_RTOS_USE_MONITOR == 1)

/**
 * @brief ����ϵͳ��������
 *        �˺����ᴴ�����������񣬲�֪ͨ��־ϵͳ���롰������ģʽ��
 *        ���������񽫽ӹ����д�����������ն���ʽѭ��ˢ��ϵͳ״̬��
 * @return 0 �ɹ�, -1 ʧ�� (�������������л򴴽�ʧ��).
 */
int MyRTOS_Monitor_Start(void);

/**
 * @brief ֹͣϵͳ��������
 *        �˺��������ټ��������񲢻ָ���������־I/O�����
 * @return 0 �ɹ�, -1 ʧ�� (������δ������).
 */
int MyRTOS_Monitor_Stop(void);

/**
 * @brief ��ѯ�������Ƿ��������С�
 * @return 1 ��������, 0 ��ֹͣ.
 */
int MyRTOS_Monitor_IsRunning(void);


#endif // MY_RTOS_USE_MONITOR

#endif // MYRTOS_MONITOR_H