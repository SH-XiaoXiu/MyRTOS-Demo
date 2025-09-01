/**
 * @file  MyRTOS_Monitor_Standards.h
 * @brief MyRTOS ��ط��� - ��׼����
 * @details �����˼�ط��������һЩ��׼���ͺͺ���ԭ�ͣ����ڽ��
 */
#ifndef MYRTOS_MONITOR_STANDARDS_H
#define MYRTOS_MONITOR_STANDARDS_H

#include "MyRTOS_Service_Config.h"

#ifndef  MYRTOS_SERVICE_MONITOR_ENABLE
#define MYRTOS_SERVICE_MONITOR_ENABLE 0
#endif

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1

#include <stdint.h>

/**
 * @brief ��ȡ�߾��ȶ�ʱ������ֵ�ĺ���ԭ�͡�
 * @details �˺���Ӧ��ƽ̨�����(PAL)��BSP��ʵ�֣���ͨ������ע��ķ�ʽ�ṩ�����ģ�顣
 *          ���ھ�ȷ������������ʱ�䡣
 * @return uint32_t ��ǰ�߾��ȶ�ʱ���ļ���ֵ��
 */
typedef uint32_t (*MonitorGetHiresTimerValueFn)(void);

#endif

#endif
