//
// Created by XiaoXiu on 8/30/2025.
//


#ifndef PLATFORM_HIRES_TIMER_H
#define PLATFORM_HIRES_TIMER_H

#include <stdint.h>

/**
 * @brief ��ʼ���߾���Ӳ����ʱ����
 *        ����һ��Ӳ����ʱ����Ϊ�������еļ�������
 */
void Platform_HiresTimer_Init(void);

/**
 * @brief ��ȡ�߾��ȶ�ʱ���ĵ�ǰ����ֵ��
 *        �˺������� MonitorGetHiresTimerValueFn ��׼��
 * @return uint32_t 32λ����ֵ��
 */
uint32_t Platform_Timer_GetHiresValue(void);


#endif // PLATFORM_HIRES_TIMER_H