#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>
#include <stddef.h>
#include "MyRTOS_Config.h"

// -----------------------------
// ʱ��ת����
// -----------------------------
/**
 * @brief ������ת��Ϊϵͳʱ�ӽ�����
 * @param ms ������
 * @return ��Ӧ��ϵͳʱ�ӽ�����
 */
#define MS_TO_TICKS(ms)         (((uint64_t)(ms) * MYRTOS_TICK_RATE_HZ) / 1000)

/**
 * @brief ��ϵͳʱ�ӽ�����ת��Ϊ����
 * @param tick ϵͳʱ�ӽ�����
 * @return ��Ӧ�ĺ�����
 */
#define TICK_TO_MS(tick)        (((uint64_t)(tick) * 1000) / MYRTOS_TICK_RATE_HZ)

// -----------------------------
// ǰ������ (Opaque Pointers)
// -----------------------------
struct Task_t;
struct Mutex_t;
struct Queue_t;
struct Semaphore_t;

// -----------------------------
// ����״̬ö��
// -----------------------------
/**
 * @brief ����״̬ö������
 */
typedef enum {
    TASK_STATE_UNUSED = 0, //����δʹ��
    TASK_STATE_READY, //�������״̬
    TASK_STATE_DELAYED, //������ʱ״̬
    TASK_STATE_BLOCKED //��������״̬
} TaskState_t;


/**
 * @brief �ں˴�������ö��
 */
typedef enum {
    KERNEL_ERROR_NONE = 0, //�޴���
    KERNEL_ERROR_STACK_OVERFLOW, //ջ�������
    KERNEL_ERROR_MALLOC_FAILED, //�ڴ����ʧ��
    KERNEL_ERROR_HARD_FAULT, //Ӳ������
} KernelErrorType_t;

// -----------------------------
// ���Ķ�����
// -----------------------------
typedef struct Task_t *TaskHandle_t; //������
typedef struct Mutex_t *MutexHandle_t; //���������
typedef void *QueueHandle_t; //���о��
typedef struct Semaphore_t *SemaphoreHandle_t; //�ź������

// -----------------------------
// ȫ���ں˱���
// -----------------------------
extern TaskHandle_t currentTask; //��ǰ���е�����
extern TaskHandle_t idleTask; //��������
extern volatile uint8_t g_scheduler_started; //�������Ƿ�������

// =============================
// System Core API
// =============================
/**
 * @brief ��ʼ��MyRTOS�ں�
 * @details ��ʼ���ں�������ݽṹ�ͱ���
 */
void MyRTOS_Init(void);

/**
 * @brief ִ���������
 * @details �����������ȼ���״̬ѡ����һ��Ҫ���е�����
 */
void MyRTOS_Schedule(void);

/**
 * @brief �������������
 * @param idle_task_hook ���������Ӻ���ָ��
 */
void Task_StartScheduler(void (*idle_task_hook)(void *));

/**
 * @brief ��ȡϵͳ��ǰʱ�ӽ�����
 * @return ��ǰϵͳʱ�ӽ�����
 */
uint64_t MyRTOS_GetTick(void);

/**
 * @brief ϵͳʱ�ӽ��Ĵ�����
 * @details ��ϵͳ��ʱ���жϵ��ã����ڸ���ϵͳʱ�Ӻʹ�����ʱ����
 */
int MyRTOS_Tick_Handler(void);

/**
 * @brief ���������Ƿ���������
 * @return 1��ʾ�������������У�0��ʾδ����
 */
uint8_t MyRTOS_Schedule_IsRunning(void);

/**
 * @brief �����ں����ش���
 *        �˺�����ƽ̨����ڲ������ã�����֪ͨ�ں˷����������¼���
 *        Ȼ�����ڹ����¼�����ʽ�㲥��ȥ��
 * @param error_type �����Ĵ������͡�
 * @param p_context  ָ���������ض����ݵ�ָ�루���磬����ջ�����
 *                   ָ��Υ�������TCB����
 */

void MyRTOS_ReportError(KernelErrorType_t error_type, void *p_context);

// =============================
// �ں��ڴ���� API
// =============================
/**
 * @brief �ں��ڴ���亯��
 * @param wantedSize ��Ҫ������ڴ��С(�ֽ�)
 * @return �ɹ�ʱ����ָ������ڴ��ָ�룬ʧ��ʱ����NULL
 */
void *MyRTOS_Malloc(size_t wantedSize);

/**
 * @brief �ں��ڴ��ͷź���
 * @param pv ָ����Ҫ�ͷŵ��ڴ���ָ��
 */
void MyRTOS_Free(void *pv);

// =============================
// ������� API
// =============================
/**
 * @brief ����һ��������
 * @param func ������ָ��
 * @param taskName ��������
 * @param stack_size �����ջ��С(�ֽ�)
 * @param param ���ݸ��������Ĳ���
 * @param priority �������ȼ�
 * @return �ɹ�ʱ������������ʧ��ʱ����NULL
 */
TaskHandle_t Task_Create(void (*func)(void *), const char *taskName, uint16_t stack_size, void *param,
                         uint8_t priority);

/**
 * @brief ɾ��ָ������
 * @param task_h Ҫɾ����������
 * @return 0��ʾ�ɹ�����0��ʾʧ��
 */
int Task_Delete(TaskHandle_t task_h);

/**
 * @brief ��ʱ��ǰ����ָ����ʱ�ӽ�����
 * @param tick ��Ҫ��ʱ��ʱ�ӽ�����
 */
void Task_Delay(uint32_t tick);

/**
 * @brief ָ֪ͨ������������
 * @param task_h ��֪ͨ��������
 * @return 0��ʾ�ɹ�����0��ʾʧ��
 */
int Task_Notify(TaskHandle_t task_h);

/**
 * @brief ���жϷ���������ָ֪ͨ������������
 * @param task_h ��֪ͨ��������
 * @param higherPriorityTaskWoken ����ָʾ�Ƿ��и������ȼ����񱻻���
 * @return 0��ʾ�ɹ�����0��ʾʧ��
 */
int Task_NotifyFromISR(TaskHandle_t task_h, int *higherPriorityTaskWoken);

/**
 * @brief ��ǰ�������ȴ�״̬
 * @details ����ǰ������������״̬��ֱ��������������жϻ���
 */
void Task_Wait(void);

/**
 * @brief ��ȡָ������ĵ�ǰ״̬
 * @param task_h ������
 * @return ����ĵ�ǰ״̬
 */
TaskState_t Task_GetState(TaskHandle_t task_h);

/**
 * @brief ��ȡָ����������ȼ�
 * @param task_h ������
 * @return �������ȼ�
 */
uint8_t Task_GetPriority(TaskHandle_t task_h);

/**
 * @brief ��ȡ��ǰ��������ľ��
 * @return ��ǰ����ľ��
 */
TaskHandle_t Task_GetCurrentTaskHandle(void);

/**
 * @brief ��ȡָ�������ID
 * @param task_h ������
 * @return ����ID
 */
uint32_t Task_GetId(TaskHandle_t task_h);


// =============================
// ���й��� API
// =============================
/**
 * @brief ����һ������
 * @param length ���г���(�ɴ洢����Ŀ��)
 * @param itemSize ������ÿ����Ŀ�Ĵ�С(�ֽ�)
 * @return �ɹ�ʱ���ض��о����ʧ��ʱ����NULL
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

/**
 * @brief ɾ��ָ������
 * @param delQueue Ҫɾ���Ķ��о��
 */
void Queue_Delete(QueueHandle_t delQueue);

/**
 * @brief ����з�������
 * @param queue ���о��
 * @param item ָ��Ҫ�������ݵ�ָ��
 * @param block_ticks �ڶ�����ʱ�ȴ���ʱ�ӽ�����(0��ʾ���ȴ�)
 * @return 0��ʾ�ɹ�����0��ʾʧ�ܻ�ʱ
 */
int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks);

/**
 * @brief �Ӷ��н�������
 * @param queue ���о��
 * @param buffer ���ڴ洢�������ݵĻ�����ָ��
 * @param block_ticks �ڶ��п�ʱ�ȴ���ʱ�ӽ�����(0��ʾ���ȴ�)
 * @return 0��ʾ�ɹ�����0��ʾʧ�ܻ�ʱ
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks);

// =============================
// ���������� API
// =============================
/**
 * @brief ����һ��������
 * @return �ɹ�ʱ���ػ����������ʧ��ʱ����NULL
 */
MutexHandle_t Mutex_Create(void);

/**
 * @brief ɾ��ָ��������
 * @param mutex Ҫɾ���Ļ��������
 */
void Mutex_Delete(MutexHandle_t mutex);

/**
 * @brief ��ȡ������(����)
 * @param mutex ���������
 */
void Mutex_Lock(MutexHandle_t mutex);

/**
 * @brief ��ָ��ʱ���ڳ��Ի�ȡ������
 * @param mutex ���������
 * @param block_ticks �ȴ������ʱ�ӽ�����(0��ʾ���ȴ�)
 * @return 0��ʾ�ɹ���ȡ������0��ʾʧ�ܻ�ʱ
 */
int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks);

/**
 * @brief �ͷŻ�����
 * @param mutex ���������
 */
void Mutex_Unlock(MutexHandle_t mutex);

/**
 * @brief ��ȡ�ݹ黥����
 * @param mutex ���������
 * @note ͬһ��������Զ�λ�ȡ�ݹ黥����
 */
void Mutex_Lock_Recursive(MutexHandle_t mutex);

/**
 * @brief �ͷŵݹ黥����
 * @param mutex ���������
 * @note ������Mutex_Lock_Recursive���ʹ��
 */
void Mutex_Unlock_Recursive(MutexHandle_t mutex);

// =============================
// �ź������� API
// =============================
/**
 * @brief ����һ���ź���
 * @param maxCount �ź���������ֵ
 * @param initialCount �ź�����ʼ����ֵ
 * @return �ɹ�ʱ�����ź��������ʧ��ʱ����NULL
 */
SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount);

/**
 * @brief ɾ��ָ���ź���
 * @param semaphore Ҫɾ�����ź������
 */
void Semaphore_Delete(SemaphoreHandle_t semaphore);

/**
 * @brief ��ȡ�ź���
 * @param semaphore �ź������
 * @param block_ticks �ȴ������ʱ�ӽ�����(0��ʾ���ȴ�)
 * @return 0��ʾ�ɹ���ȡ�ź�������0��ʾʧ�ܻ�ʱ
 */
int Semaphore_Take(SemaphoreHandle_t semaphore, uint32_t block_ticks);

/**
 * @brief �ͷ��ź���
 * @param semaphore �ź������
 * @return 0��ʾ�ɹ�����0��ʾʧ��
 */
int Semaphore_Give(SemaphoreHandle_t semaphore);

/**
 * @brief ���жϷ����������ͷ��ź���
 * @param semaphore �ź������
 * @param higherPriorityTaskWoken ����ָʾ�Ƿ��и������ȼ����񱻻���
 * @return 0��ʾ�ɹ�����0��ʾʧ��
 */
int Semaphore_GiveFromISR(SemaphoreHandle_t semaphore, int *higherPriorityTaskWoken);


#endif // MYRTOS_H
