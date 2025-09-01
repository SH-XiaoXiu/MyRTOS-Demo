/**
 * @file  MyRTOS_Monitor.h
 * @brief MyRTOS ��ط��� - �����ӿ�
 * @details �ṩ��ȡ��������ʱ״̬��CPUʹ���ʡ�ջʹ�úͶ��ڴ�ͳ�ƵĹ��ܡ�
 */
#ifndef MYRTOS_MONITOR_H
#define MYRTOS_MONITOR_H

#include "MyRTOS_Service_Config.h"

#ifndef  MYRTOS_SERVICE_MONITOR_ENABLE
#define MYRTOS_SERVICE_MONITOR_ENABLE 0
#endif

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1

#include "MyRTOS.h"
#include "MyRTOS_Monitor_Standards.h"

/**
 * @brief ����ͳ����Ϣ�ṹ�� (���Ⱪ¶)��
 */
typedef struct {
    TaskHandle_t task_handle;           // ������
    const char *task_name;              // ��������
    TaskState_t state;                  // ��ǰ״̬ (Running, Ready, Blocked...)
    uint8_t current_priority;           // ��ǰ���ȼ� (���������ȼ��̳ж��ı�)
    uint8_t base_priority;              // �������ȼ�
    uint32_t stack_size_bytes;          // ��ջ��С (�ֽ�)
    uint32_t stack_high_water_mark_bytes; // ջ��ʷ���ʹ���� (�ֽ�)��ֵԽС��ʾʣ��ջ�ռ�Խ��
    uint64_t total_runtime;             // ������ʱ�� (��λ: �߾���timer ticks)
    uint32_t cpu_usage_permille;        // CPUʹ���� (ǧ�ֱ�)�����ɵ�����������ʱ����ϼ����ֵ�ó�
} TaskStats_t;


/**
 * @brief ���ڴ�ͳ����Ϣ�ṹ�� (���Ⱪ¶)��
 */
typedef struct {
    size_t total_heap_size;             // �ܶѴ�С (�ֽ�)
    size_t free_bytes_remaining;        // ��ǰʣ������ֽ���
    size_t minimum_ever_free_bytes;     // ��ʷ��Сʣ���ֽ��� (�ѵ�ˮ��)
} HeapStats_t;


/**
 * @brief Monitor ��ʼ�����ýṹ�� (��������ע��)��
 */
typedef struct {
    /** �����ṩһ����ȡ�߾���ʱ��ֵ�ĺ���ָ�룬����ͳ����������ʱ�� */
    MonitorGetHiresTimerValueFn get_hires_timer_value;
} MonitorConfig_t;

/**
 * @brief ��ʼ��Monitor����
 * @details �˺��������ں�ע���¼����������Ա�����ʽ�ռ�ͳ�����ݣ�������С��
 * @param config [in] ָ��Monitor���õ�ָ�롣
 * @return int 0 �ɹ�, -1 ʧ�� (������Ϊ��)��
 */
int Monitor_Init(const MonitorConfig_t *config);

/**
 * @brief (�ں�API��չ) ��ȡ���������е���һ����������
 * @details ���ڱ���ϵͳ����������
 * @param previous_handle ��һ�������
* @return TaskHandle_t ��һ������ľ��, �� NULL ����ѱ����ꡣ
 */
TaskHandle_t Monitor_GetNextTask(TaskHandle_t previous_handle);

/**
 * @brief ��ȡָ���������ϸͳ����Ϣ��
 * @param task_h      [in]  Ҫ��ѯ����������
 * @param p_stats_out [out] �������ͳ����Ϣ�Ľṹ��ָ�롣
 * @return int 0 �ɹ�, -1 ʧ�� (������Ч)��
 */
int Monitor_GetTaskInfo(TaskHandle_t task_h, TaskStats_t *p_stats_out);

/**
 * @brief ��ȡ���ڴ��ͳ����Ϣ��
 * @param p_stats_out [out] ��������ͳ����Ϣ�Ľṹ��ָ�롣
 */
void Monitor_GetHeapStats(HeapStats_t *p_stats_out);

#endif // MYRTOS_MONITOR_ENABLE

#endif // MYRTOS_MONITOR_H
