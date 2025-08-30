#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>
#include <stddef.h>
#include "MyRTOS_Config.h"
#include "MyRTOS_IO.h"

// -----------------------------
// ʱ��ת����
// -----------------------------
#define MS_TO_TICKS(ms)         (((uint64_t)(ms) * MY_RTOS_TICK_RATE_HZ) / 1000)
#define TICK_TO_MS(tick)        (((uint64_t)(tick) * 1000) / MY_RTOS_TICK_RATE_HZ)


// -----------------------------
// ǰ������ (Opaque Pointers)
// -----------------------------
// �ں˶���ṹ����ⲿ�ǲ�͸���ģ�ֻ��ͨ���������
struct Task_t;
struct Mutex_t;
struct Timer_t;
struct Queue_t;
struct Semaphore_t;

#if (MY_RTOS_USE_STDIO == 1)
struct Stream_t;
#endif

// -----------------------------
// ����״̬ö��
// -----------------------------
typedef enum {
    TASK_STATE_UNUSED = 0, // ������ƿ�δʹ��
    TASK_STATE_READY, // �������
    TASK_STATE_DELAYED, // ������ʱ��
    TASK_STATE_BLOCKED // ������ȴ��¼�����
} TaskState_t;

// -----------------------------
// ���Ķ�����
// -----------------------------
typedef struct Task_t *TaskHandle_t;
typedef struct Mutex_t *MutexHandle_t;
#if (MY_RTOS_USE_SOFTWARE_TIMERS == 1)
typedef void (*TimerCallback_t)(struct Timer_t *timer);

typedef struct Timer_t *TimerHandle_t;
#endif
typedef void *QueueHandle_t; // Queue remains slightly more abstract for now
typedef struct Semaphore_t *SemaphoreHandle_t;

// -----------------------------
// ȫ���ں˱���
// -----------------------------
extern TaskHandle_t currentTask;
extern TaskHandle_t idleTask;
extern volatile uint8_t g_scheduler_started;

// =============================
// System Core API
// =============================
void MyRTOS_SystemInit(void);

void MyRTOS_Init(void);

void Task_StartScheduler(void);

uint64_t MyRTOS_GetTick(void);

void MyRTOS_Tick_Handler(void);

uint8_t MyRTOS_Schedule_IsRunning(void);

// =============================
// Memory Management API
// =============================
void *MyRTOS_Malloc(size_t wantedSize);

void MyRTOS_Free(void *pv);

// =============================
// Task Management API
// =============================
TaskHandle_t Task_Create(void (*func)(void *), const char *taskName, uint16_t stack_size, void *param,
                         uint8_t priority);

int Task_Delete(TaskHandle_t task_h);

void Task_Delay(uint32_t tick);

int Task_Notify(TaskHandle_t task_h);

int Task_NotifyFromISR(TaskHandle_t task_h, int *higherPriorityTaskWoken);

void Task_Wait(void);

TaskState_t Task_GetState(TaskHandle_t task_h);

uint8_t Task_GetPriority(TaskHandle_t task_h);

TaskHandle_t Task_GetCurrentTaskHandle(void);

#if (MY_RTOS_USE_STDIO == 1)
/**
 * @brief ��ȡָ������ı�׼��������
 * @param task_h ������ (��ΪNULL�����ȡ��ǰ�������)
 * @return ָ�������stdin����ָ��
 */
Stream_t *Task_GetStdIn(TaskHandle_t task_h);

/**
 * @brief ��ȡָ������ı�׼�������
 * @param task_h ������ (��ΪNULL�����ȡ��ǰ�������)
 * @return ָ�������stdout����ָ��
 */
Stream_t *Task_GetStdOut(TaskHandle_t task_h);

/**
 * @brief ��ȡָ������ı�׼��������
 * @param task_h ������ (��ΪNULL�����ȡ��ǰ�������)
 * @return ָ�������stderr����ָ��
 */
Stream_t *Task_GetStderr(TaskHandle_t task_h);
#endif

/**
 * @brief ��ȡ��ǰ����ĵ�����Ϣ����Ҫ�� HardFault Handler ʹ�á�
 *        ����һ���Ǳ�׼�ĵײ㺯����Ӧ�ó���Ӧ���á�
 * @param pTaskNameBuffer   ���ڴ洢�������Ļ�����ָ��
 * @param bufferSize        ��������С
 * @param pTaskId           ���ڴ洢����ID��ָ��
 * @return 1 ����ɹ���ȡ��Ϣ, 0 ���û�е�ǰ����
 */
int Task_GetDebugInfo(char *pTaskNameBuffer, size_t bufferSize, uint32_t *pTaskId);


// =============================
// Queue Management API
// =============================
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

void Queue_Delete(QueueHandle_t delQueue);

int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks);

int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks);

// =============================
// Timer Management API (Software Timers)
// =============================
#if (MY_RTOS_USE_SOFTWARE_TIMERS == 1)
TimerHandle_t Timer_Create(uint32_t delay, uint32_t period, TimerCallback_t callback, void *arg);

int Timer_Start(TimerHandle_t timer);

int Timer_Stop(TimerHandle_t timer);

int Timer_Delete(TimerHandle_t timer);
#endif

// =============================
// Mutex Management API
// =============================
MutexHandle_t Mutex_Create(void);

void Mutex_Lock(MutexHandle_t mutex);

int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks);

void Mutex_Unlock(MutexHandle_t mutex);

void Mutex_Lock_Recursive(MutexHandle_t mutex);

void Mutex_Unlock_Recursive(MutexHandle_t mutex);

// =============================
// Semaphore Management API
// =============================
SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount);

void Semaphore_Delete(SemaphoreHandle_t semaphore);

int Semaphore_Take(SemaphoreHandle_t semaphore, uint32_t block_ticks);

int Semaphore_Give(SemaphoreHandle_t semaphore);

int Semaphore_GiveFromISR(SemaphoreHandle_t semaphore, int *higherPriorityTaskWoken);


// =============================
// ����ʱͳ�� API
// =============================
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
typedef struct TaskStats_t {
    uint32_t taskId;
#if (MY_RTOS_TASK_NAME_MAX_LEN > 0)
    char taskName[MY_RTOS_TASK_NAME_MAX_LEN];
#endif
    TaskState_t state;
    uint8_t currentPriority;
    uint8_t basePriority;
    uint64_t runTimeCounter;
    uint32_t stackHighWaterMark;
    uint32_t stackSize;
} TaskStats_t;

typedef struct HeapStats_t {
    size_t totalHeapSize;
    size_t freeBytesRemaining;
    size_t minimumEverFreeBytesRemaining;
} HeapStats_t;

void Task_GetInfo(TaskHandle_t taskHandle, TaskStats_t *pTaskStats);

TaskHandle_t Task_GetNextTaskHandle(TaskHandle_t lastTaskHandle);

void Heap_GetStats(HeapStats_t *pHeapStats);
#endif

// =============================
// �ں˹��� API
// =============================

// �ں���־���Ӻ���������
typedef void (*KernelLogHook_t)(const char *message, uint16_t length);

void MyRTOS_RegisterKernelLogHook(KernelLogHook_t pfnHook);

#endif // MYRTOS_H
