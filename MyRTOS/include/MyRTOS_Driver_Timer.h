//
// Created by XiaoXiu on 8/29/2025.
//

#ifndef MYRTOS_DRIVER_TIMER_H
#define MYRTOS_DRIVER_TIMER_H

#include <stdint.h>
#include "MyRTOS_Config.h"

// ������ú��Ƿ���ڣ�������������һ���յ�Ĭ��ֵ����ֹ�������
#ifndef MY_RTOS_TIMER_DEVICE_LIST
#define MY_RTOS_TIMER_DEVICE_LIST
#endif

// �������ú� MY_RTOS_TIMER_DEVICE_LIST �Զ����ɶ�ʱ��ID��ö��
#define USE_TIMER(id, dev, param) TIMER_ID_##id,
typedef enum {
    MY_RTOS_TIMER_DEVICE_LIST
    TIMER_ID_MAX,
    TIMER_ID_FORCE_INT = 0x7FFFFFFF // ȷ��ö��������int��С
} TimerID_t;
#undef USE_TIMER // ʹ���������ȡ������

// ��ʱ����� (Opaque handle)
typedef void* TimerHandle_dev_t;

/**
 * @brief ��ʼ���� MyRTOS_Config.h �ж�������ж�ʱ��
 *        �˺����� MyRTOS_SystemInit �Զ�����
 */
void MyRTOS_Timer_Init(void);

/**
 * @brief �����߼�ID��ȡָ���Ķ�ʱ�����
 * @param id ��ʱ���߼�ID
 * @return �ɹ����ؾ����ʧ�ܷ���NULL
 */
TimerHandle_dev_t MyRTOS_Timer_GetHandle(TimerID_t id);

/**
 * @brief ����һ����ʱ��
 * @param handle ��ʱ�����
 * @return 0 �ɹ�
 */
int MyRTOS_Timer_Start(TimerHandle_dev_t handle);

/**
 * @brief ֹͣһ����ʱ��
 * @param handle ��ʱ�����
 * @return 0 �ɹ�
 */
int MyRTOS_Timer_Stop(TimerHandle_dev_t handle);

/**
 * @brief ��ȡ��ʱ���ĵ�ǰ����ֵ
 * @param handle ��ʱ�����
 * @return 32λ����ֵ
 */
uint32_t MyRTOS_Timer_GetCount(TimerHandle_dev_t handle);

/**
 * @brief ���ö�ʱ�������� (�Զ���װ��ֵ)
 * @param handle ��ʱ�����
 * @param period ����ֵ
 * @return 0 �ɹ�
 */
int MyRTOS_Timer_SetPeriod(TimerHandle_dev_t handle, uint32_t period);

/**
 * @brief ��ȡ��ʱ���ļ���Ƶ�� (Hz)
 * @param handle ��ʱ�����
 * @return Ƶ�� (Hz)
 */
uint32_t MyRTOS_Timer_GetFreq(TimerHandle_dev_t handle);

#endif // MYRTOS_DRIVER_TIMER_H