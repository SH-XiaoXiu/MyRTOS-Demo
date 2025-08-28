//
// Created by XiaoXiu on 8/28/2025.
//

#ifndef MYRTOS_PORT_H
#define MYRTOS_PORT_H
#include <stdint.h>


/**
 * @brief ��ʼ��ƽ̨��ص�Ӳ����
 *        ͨ�����ڳ�ʼ�����Դ��ڵȡ�
 */
void MyRTOS_Platform_Init(void);

/**
 * @brief ͨ��Ӳ������һ���ַ���
 *        ���������ϲ�I/O��������ճ��ڡ�
 * @param c Ҫ���͵��ַ���
 */
void MyRTOS_Platform_PutChar(char c);

//TODO ��û������
/**
 * @brief ��ʼ���������Ӳ�� (��: USART)��
 */
void MyRTOS_Platform_Debug_Init(void);


/**
 * @brief ��ʼ��������ϵͳTick��ʱ����
 * @param tick_rate_hz ϵͳTickƵ�ʡ�
 * @return 0 �ɹ�, -1 ʧ��.
 */
int MyRTOS_Platform_Tick_Init(uint32_t tick_rate_hz);

/**
 * @brief ���� PendSV �� SysTick ���ж����ȼ���Ϊ��������׼����
 */
void MyRTOS_Platform_Scheduler_Init(void);

/**
 * @brief ��ʼ���û�����Ӳ�� (��: �����ж�)��
 *        �������Ӧ�������жϣ������жϴ������е���һ���ص���
 * @param callback �������¼�����ʱҪ���õĺ���ָ�롣
 */
void MyRTOS_Platform_UserInput_Init(void (*callback)(void));

/**
 * @brief ��ʼ��һ���߷ֱ��ʶ�ʱ����
 */
void MyRTOS_Platform_PerfTimer_Init(void);

/**
 * @brief ��ȡ�߷ֱ��ʶ�ʱ���ĵ�ǰ����ֵ��
 */
uint32_t MyRTOS_Platform_PerfTimer_GetCount(void);

#endif // MYRTOS_PORT_H