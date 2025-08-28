//
// Created by XiaoXiu on 8/29/2025.
//

#ifndef MYRTOS_PLATFORM_H
#define MYRTOS_PLATFORM_H

/**
 * @brief ��ʼ��ƽ̨��ص�Ӳ������ʱ�ӡ����Դ��ڵȡ�
 *        �˺���Ӧ��RTOS����������ǰ����main�����б����á�
 */
void MyRTOS_Platform_Init(void);

/**
 * @brief ͨ��Ӳ������һ���ַ���
 *        ���������ϲ�I/O������Log, Monitor�������ճ��ڡ�
 * @param c Ҫ���͵��ַ���
 */
void MyRTOS_Platform_PutChar(char c);

#endif // MYRTOS_PLATFORM_H