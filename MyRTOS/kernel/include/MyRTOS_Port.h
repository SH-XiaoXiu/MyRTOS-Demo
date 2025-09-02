#ifndef MYRTOS_PORT_H
#define MYRTOS_PORT_H

#include <stdint.h>

// ƽ̨�޹ص����ݽṹ
typedef uint32_t StackType_t; // CPUջ����Ȼ���
typedef int32_t BaseType_t; // CPU���з������ſ��
typedef uint32_t UBaseType_t; // CPU���޷������ſ��


// ��ֲ�㺯��ԭ��
/**
 * @brief ��ʼ�������ջ֡��
 * @param pxTopOfStack ����ջ����ߵ�ַ��
 * @param pxCode ����������ڵ�ַ��
 * @param pvParameters ���ݸ�����Ĳ�����
 * @return ���س�ʼ���������ջ��ָ�� (SP)��
 */
StackType_t *MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters);

/**
 * @brief ����RTOS��������
 *        �˺�����������Ӳ����������һ����������Զ���᷵�ء�
 * @return ����ɹ������򲻷��أ����ʧ���򷵻ط���ֵ��
 */
BaseType_t MyRTOS_Port_StartScheduler(void);

/**
 * @brief �����ٽ�����
 *        ������ƽ̨��ֲ��ʵ�֣��Ա�֤������ԭ���ԡ�
 */
void MyRTOS_Port_EnterCritical(void);

/**
 * @brief �˳��ٽ�����
 */
void MyRTOS_Port_ExitCritical(void);

/**
 * @brief ������������������һ���������л���
 */
void MyRTOS_Port_Yield(void);

/**
 * @brief ���жϷ������(ISR)������������һ���������л���
 * @param higherPriorityTaskWoken �����ISR�л�����һ���������ȼ�������
 *                                 ��˱���ӦΪ����ֵ����ʱ����Ҫ�����л���
 */
void MyRTOS_Port_YieldFromISR(BaseType_t higherPriorityTaskWoken);

#endif // MYRTOS_PORT_H
