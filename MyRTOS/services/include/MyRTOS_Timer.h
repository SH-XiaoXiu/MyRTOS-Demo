/**
 * @brief MyRTOS �����ʱ������ - �����ӿ�
 * @details �ṩ���ں�̨����ġ��߾��ȵ������ʱ�����ܡ�
 */

#ifndef MYRTOS_EXT_TIMER_H
#define MYRTOS_EXT_TIMER_H

#include "MyRTOS_Service_Config.h"


#ifndef MYRTOS_SERVICE_TIMER_ENABLE
#define MYRTOS_SERVICE_TIMER_ENABLE 0
#endif


#if MYRTOS_SERVICE_TIMER_ENABLE == 1

#include <stdint.h>

// ǰ��������ʱ���ṹ�壬���ⲿ��͸����ʵ����Ϣ����
struct Timer_t;

/** @brief �����ʱ��������͡�*/
typedef struct Timer_t *TimerHandle_t;

/**
 * @brief ��ʱ���ص���������
 * @param timer �����ص��Ķ�ʱ�����
 */
typedef void (*TimerCallback_t)(TimerHandle_t timer);

/**
 * @brief ��ʼ�������ʱ������
 * @details ������ʹ���κ�������ʱ��API֮ǰ���ô˺�����
 *          ���ᴴ��һ��ר�õġ���ʱ�������������������ж�ʱ����
 * @param timer_task_priority ��ʱ��������������ȼ���ͨ��Ӧ��Ϊ�ϸ����ȼ���
 *                            ��ȷ����ʱ���ص��ļ�ʱ�ԡ�
 * @param timer_task_stack_size ��ʱ�����������ջ��С��
 * @return int 0 �ɹ�, -1 ʧ�ܡ�
 */
int TimerService_Init(uint8_t timer_task_priority, uint16_t timer_task_stack_size);

/**
 * @brief ����һ�������ʱ����
 * @param name          [in] ��ʱ�������ƣ����ڵ��ԣ���
 * @param period        [in] ��ʱ�������ڣ���λ��ticks�������ڵ��ζ�ʱ�������Ǵ�����ʱ��
 * @param is_periodic   [in] 1 ��ʾΪ�����Զ�ʱ��, 0 ��ʾΪ���ζ�ʱ����
 * @param callback      [in] ��ʱ������ʱҪִ�еĻص�������
 * @param p_timer_arg   [in] ���ݸ��ص��������Զ��������
 * @return TimerHandle_t �ɹ��򷵻ض�ʱ�������ʧ���򷵻� NULL��
 */
TimerHandle_t Timer_Create(const char *name, uint32_t period, uint8_t is_periodic, TimerCallback_t callback,
                           void *p_timer_arg);

/**
 * @brief ����һ�������ʱ����
 * @details ��ʱ�����ڴӴ˿��� `period` ticks ���״δ�����
 * @param timer       [in] Ҫ�����Ķ�ʱ�������
 * @param block_ticks [in] ���������ʱ������ʱ�������ȴ�ʱ�䡣0��ʾ���ȴ���
 * @return int 0 ����ͳɹ�, -1 ʧ�ܡ�
 */
int Timer_Start(TimerHandle_t timer, uint32_t block_ticks);

/**
 * @brief ֹͣһ�������ʱ����
 * @param timer       [in] Ҫֹͣ�Ķ�ʱ�������
 * @param block_ticks [in] ���������ʱ������ʱ�������ȴ�ʱ�䡣0��ʾ���ȴ���
 * @return int 0 ����ͳɹ�, -1 ʧ�ܡ�
 */
int Timer_Stop(TimerHandle_t timer, uint32_t block_ticks);

/**
 * @brief ɾ��һ�������ʱ����
 * @param timer       [in] Ҫɾ���Ķ�ʱ�������
 * @param block_ticks [in] ���������ʱ������ʱ�������ȴ�ʱ�䡣0��ʾ���ȴ���
 * @return int 0 ����ͳɹ�, -1 ʧ�ܡ�
 */
int Timer_Delete(TimerHandle_t timer, uint32_t block_ticks);

/**
 * @brief ���Ķ�ʱ�������ڡ�
 * @param timer       [in] Ҫ�����Ķ�ʱ�������
 * @param new_period  [in] �µ����ڣ���λ��ticks����
 * @param block_ticks [in] ���������ʱ������ʱ�������ȴ�ʱ�䡣0��ʾ���ȴ���
 * @return int 0 ����ͳɹ�, -1 ʧ�ܡ�
 */
int Timer_ChangePeriod(TimerHandle_t timer, uint32_t new_period, uint32_t block_ticks);

/**
 * @brief ��ȡ��ʱ������ʱ�󶨵��û�������
 * @param timer [in] ��ʱ�������
 * @return void* �洢�ڶ�ʱ���е��û�����ָ�롣
 */
void *Timer_GetArg(TimerHandle_t timer);

#endif // MYRTOS_TIMER_ENABLE

#endif // MYRTOS_EXT_TIMER_H
