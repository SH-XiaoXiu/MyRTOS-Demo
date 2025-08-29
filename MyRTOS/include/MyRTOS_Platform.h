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


/**
 * @brief ��Ӳ����ȡһ���ַ�������������
 *        �˺����������ء�
 * @param c ָ�����ڴ洢�����ַ��ı�����ָ�롣
 * @return 1 ��ʾ�ɹ���ȡһ���ַ�, 0 ��ʾ��ǰû�п����ַ���
 */
int MyRTOS_Platform_GetChar(char *c);

/**
 * @brief ��Ӳ����ȡһ���ַ�������ʽ����
 *        �˺�������������ֱ�����յ�һ�����ַ���
 *        ��������������������Terminal��������ѡ��
 * @param c ָ�����ڴ洢�����ַ��ı�����ָ�롣
 * @return 1 ��ʾ�ɹ���ȡһ���ַ�, 0 ��ʾ��ʱ��ʧ�� (�ڴ�ʵ��������Ϊ0)��
 */
int MyRTOS_Platform_GetChar_Blocking(char *c);


#endif // MYRTOS_PLATFORM_H