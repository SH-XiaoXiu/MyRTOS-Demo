#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>
#include "MyRTOS_Config.h" // �û��������ļ����������й��ܿ���

// -----------------------------
// ʱ��ת����
// -----------------------------
// ����תtick
#define MS_TO_TICKS(ms)         (((uint64_t)(ms) * MY_RTOS_TICK_RATE_HZ) / 1000)
// tickת����
#define TICK_TO_MS(tick)        (((uint64_t)(tick) * 1000) / MY_RTOS_TICK_RATE_HZ)

// -----------------------------
// ǰ������
// -----------------------------
struct Task_t;
struct Mutex_t;
struct Timer_t;
struct Queue_t;

// -----------------------------
// ����״̬ö��
// -----------------------------
typedef enum {
    TASK_STATE_UNUSED = 0, // ������ƿ�δʹ��
    TASK_STATE_READY,      // �������
    TASK_STATE_DELAYED,    // ������ʱ��
    TASK_STATE_BLOCKED     // ������ȴ��¼�����
} TaskState_t;

// -----------------------------
// ���Ķ�����
// -----------------------------
typedef struct Task_t *TaskHandle_t;
typedef struct Mutex_t *MutexHandle_t;
typedef struct Timer_t *TimerHandle_t;
typedef void *QueueHandle_t;
struct Semaphore_t;
typedef struct Semaphore_t *SemaphoreHandle_t;

// -----------------------------
// ��ʱ���ص�����ָ������
// -----------------------------
typedef void (*TimerCallback_t)(TimerHandle_t timer);

// =============================
// System Core
// =============================
/**
 * @brief ͳһ��ϵͳ��ʼ��������
 *        �û��� main ��Ӧ���ȵ��ô˺�����
 */
void MyRTOS_SystemInit(void);

/**
 * @brief ��ʼ��RTOS�ں˵ĺ������ݽṹ��
 *        �˺����� MyRTOS_SystemInit �Զ����á�
 */
void MyRTOS_Init(void);

/**
 * @brief ��������������ʼ�������
 */
void Task_StartScheduler(void);

/**
 * @brief ��ȡϵͳtick����
 * @return ��ǰϵͳtick
 */
uint64_t MyRTOS_GetTick(void);

/**
 * @brief �ں˵�Tick������������ֲ���Tick�жϵ���
 */
void MyRTOS_Tick_Handler(void);

// =============================
// Task Management
// =============================
/**
 * @brief ��������
 * @param func ������ָ��
 * @param taskName �������ƣ������ڵ��Ի�ͳ��
 * @param stack_size ջ��С(��λ��word, 4 bytes)
 * @param param �������
 * @param priority �������ȼ�
 * @return ������
 */
TaskHandle_t Task_Create(void (*func)(void *),
                         const char *taskName,
                         uint16_t stack_size,
                         void *param,
                         uint8_t priority);

/**
 * @brief ɾ������
 * @param task_h ��������NULL��ʾɾ����ǰ����
 * @return 0�ɹ���-1ʧ��
 */
int Task_Delete(TaskHandle_t task_h);

/**
 * @brief ��ʱ����
 * @param tick ��ʱʱ��(tick)
 */
void Task_Delay(uint32_t tick);

/**
 * @brief ����֪ͨ������
 * @param task_h ������
 * @return 0�ɹ���-1ʧ��
 */
int Task_Notify(TaskHandle_t task_h);


/**
 * @brief [ISR��ȫ] ����֪ͨ������
 * @param task_h ������
 * @param higherPriorityTaskWoken ������ѵ��������ȼ����ߣ���ָ��ָ���ֵ������Ϊ1
 * @return 0�ɹ���-1ʧ��
 */
int Task_NotifyFromISR(TaskHandle_t task_h, int *higherPriorityTaskWoken);

/**
 * @brief �ȴ�����֪ͨ
 */
void Task_Wait(void);

/**
 * @brief ��ȡ����״̬
 * @param task_h ������
 * @return ����״̬(TaskState_t)
 */
TaskState_t Task_GetState(TaskHandle_t task_h);

/**
 * @brief ��ȡ�������ȼ�
 * @param task_h ������
 * @return ���ȼ�
 */
uint8_t Task_GetPriority(TaskHandle_t task_h);

/**
 * @brief ��ȡ��ǰ���е�������
 * @return ��ǰ������
 */
TaskHandle_t Task_GetCurrentTaskHandle(void);

// =============================
// Queue Management
// =============================
/**
 * @brief ��������
 * @param length ���г���
 * @param itemSize ����Ԫ�ش�С
 * @return ���о��
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

/**
 * @brief ɾ������
 * @param delQueue ���о��
 */
void Queue_Delete(QueueHandle_t delQueue);

/**
 * @brief ����з�������
 * @param queue ���о��
 * @param item ����������
 * @param block_ticks ����ʱ�䣬0��������MY_RTOS_MAX_DELAY���޵ȴ�
 * @return 1�ɹ���0ʧ��
 */
int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks);

/**
 * @brief �Ӷ��н�������
 * @param queue ���о��
 * @param buffer ���ջ�����
 * @param block_ticks ����ʱ�䣬0��������MY_RTOS_MAX_DELAY���޵ȴ�
 * @return 1�ɹ���0ʧ��
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks);

// =============================
// Timer Management (Software Timers)
// =============================
/**
 * @brief ������ʱ��
 * @param delay ������ʱ(tick)
 * @param period ��ʱ����(tick)��0��ʾһ���Զ�ʱ��
 * @param callback �ص�����
 * @param arg �ص���������
 * @return ��ʱ�����
 */
TimerHandle_t Timer_Create(uint32_t delay, uint32_t period, TimerCallback_t callback, void *arg);

/**
 * @brief ������ʱ��
 * @param timer ��ʱ�����
 * @return 0�ɹ���-1ʧ��
 */
int Timer_Start(TimerHandle_t timer);

/**
 * @brief ֹͣ��ʱ��
 * @param timer ��ʱ�����
 * @return 0�ɹ���-1ʧ��
 */
int Timer_Stop(TimerHandle_t timer);

/**
 * @brief ɾ����ʱ��
 * @param timer ��ʱ�����
 * @return 0�ɹ���-1ʧ��
 */
int Timer_Delete(TimerHandle_t timer);

// =============================
// Mutex Management
// =============================
/**
 * @brief ����������
 * @return �ɹ����ؾ����ʧ�ܷ���NULL
 */
MutexHandle_t Mutex_Create(void);

/**
 * @brief ��ȡ������
 */
void Mutex_Lock(MutexHandle_t mutex);

/**
 * @brief ����ʱ����
 * @param block_ticks ����tick����0��ʾ������
 * @return 1�ɹ���0��ʱʧ��
 */
int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks);

/**
 * @brief �ͷŻ�����
 */
void Mutex_Unlock(MutexHandle_t mutex);

/**
 * @brief �ݹ�����ȡ
 */
void Mutex_Lock_Recursive(MutexHandle_t mutex);

/**
 * @brief �ݹ����ͷ�
 */
void Mutex_Unlock_Recursive(MutexHandle_t mutex);




// =============================
// Semaphore Management
// =============================
/**
 * @brief ����һ�������ź���
 * @param maxCount �ź����ܴﵽ��������ֵ
 * @param initialCount �ź����ĳ�ʼ����ֵ
 * @return �ɹ������ź��������ʧ�ܷ���NULL
 */
SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount);

/**
 * @brief ɾ��һ���ź���
 * @param semaphore Ҫɾ�����ź������
 */
void Semaphore_Delete(SemaphoreHandle_t semaphore);

/**
 * @brief ��ȡ(P����)һ���ź���
 * @param semaphore �ź������
 * @param block_ticks ����ʱ��(ticks)��0��ʾ��������MY_RTOS_MAX_DELAY��ʾ���޵ȴ�
 * @return 1��ʾ�ɹ���ȡ��0��ʾ��ʱʧ��
 */
int Semaphore_Take(SemaphoreHandle_t semaphore, uint32_t block_ticks);

/**
 * @brief �ͷ�(V����)һ���ź���
 * @param semaphore �ź������
 * @return 1��ʾ�ɹ��ͷţ�0��ʾ�ź����Ѵ����ֵ
 */
int Semaphore_Give(SemaphoreHandle_t semaphore);


/**
 * @brief [ISR��ȫ] �ͷ�(V����)һ���ź���
 * @param semaphore �ź������
 * @param higherPriorityTaskWoken ������ѵ��������ȼ����ߣ���ָ��ָ���ֵ������Ϊ1
 * @return 1��ʾ�ɹ��ͷţ�0��ʾ�ź����Ѵ����ֵ
 */
int Semaphore_GiveFromISR(SemaphoreHandle_t semaphore, int *higherPriorityTaskWoken);

// =============================
// ����ʱͳ�� API
// =============================
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
/**
 * @brief ��������ͳ����Ϣ
 */
typedef struct TaskStats_t {
    uint32_t taskId;                      // ����ID
#if (MY_RTOS_TASK_NAME_MAX_LEN > 0)
    char taskName[MY_RTOS_TASK_NAME_MAX_LEN]; // ��������
#endif
    TaskState_t state;                    // ��ǰ����״̬
    uint8_t currentPriority;              // ��ǰ���ȼ�
    uint8_t basePriority;                 // �������ȼ�
    uint64_t runTimeCounter;              // �ۼ�����ʱ��(��λ:�ɸ߾��ȶ�ʱ��Ƶ�ʾ���)
    uint32_t stackHighWaterMark;          // ջˮλ(����ջ��С, bytes)
    uint32_t stackSize;                   // ջ��С(bytes)
} TaskStats_t;

/**
 * @brief ���ڴ�ͳ����Ϣ
 */
typedef struct HeapStats_t {
    size_t totalHeapSize;                 // ���ܴ�С
    size_t freeBytesRemaining;            // ��ǰʣ���ڴ�
    size_t minimumEverFreeBytesRemaining; // ��ʷ��Сʣ���ڴ�
} HeapStats_t;

/**
 * @brief ��ȡ����ͳ����Ϣ
 */
void Task_GetInfo(TaskHandle_t taskHandle, TaskStats_t *pTaskStats);

/**
 * @brief ��ȡ��һ�������������ڵ�������
 */
TaskHandle_t Task_GetNextTaskHandle(TaskHandle_t lastTaskHandle);

/**
 * @brief ��ȡ��ͳ����Ϣ
 */
void Heap_GetStats(HeapStats_t *pHeapStats);

#endif // MY_RTOS_GENERATE_RUN_TIME_STATS

#endif // MYRTOS_H