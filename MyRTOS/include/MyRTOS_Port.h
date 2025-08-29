//
// Created by XiaoXiu on 8/28/2025.
//

#ifndef MYRTOS_PORT_H
#define MYRTOS_PORT_H

#include <stdint.h>
#include "MyRTOS_Config.h"

// =======================================================================
// CPU �ܹ���ص����ͺͺ�
// =======================================================================

typedef uint32_t    StackType_t;
typedef int32_t     BaseType_t;
typedef uint32_t    UBaseType_t;



// =======================================================================
// �ں�������ջ��ʼ�� (CPU������ֲ)
// =======================================================================

/**
 * @brief ��ʼ�������ջ֡
 * @param pxTopOfStack ����ջ����ߵ�ַ
 * @param pxCode ����������ڵ�ַ
 * @param pvParameters ���ݸ�����Ĳ���
 * @return ���س�ʼ���������ջ��ָ�� (SP)
 */
StackType_t* MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters);

/**
 * @brief ����RTOS������
 *        �˺�����������ϵͳ��ʱ�� (SysTick)�������ж����ȼ�����������һ������
 *        �˺�����Զ���᷵�ء�
 * @return ����ɹ������򲻷��أ����ʧ���򷵻ط���ֵ��
 */
BaseType_t MyRTOS_Port_StartScheduler(void);

#endif // MYRTOS_PORT_H