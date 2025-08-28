#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>
#include "gd32f4xx.h" // ����оƬ��ص�ͷ�ļ���ʹ�� CMSIS ���ĺ���

//RTOS Config
#define MY_RTOS_MAX_PRIORITIES    (16)      // ���֧�ֵ����ȼ�����
#define MY_RTOS_TICK_RATE_HZ    (1000)    // ϵͳTickƵ�� (Hz)
#define MY_RTOS_MAX_DELAY       (0xFFFFFFFFU) // �����ʱticks

//Memory Management
#define RTOS_MEMORY_POOL_SIZE   (16 * 1024) // �ں�ʹ�õľ�̬�ڴ�ش�С (bytes)
#define HEAP_BYTE_ALIGNMENT     (8)         // �ڴ�����ֽ���

// ����������� (����Ϊ1����, 0�ر�)
// #define DEBUG_PRINT 0

#if DEBUG_PRINT
#include <stdio.h>
#define DBG_PRINTF(...) do {                   \
     uint32_t primask;                         \
     MY_RTOS_ENTER_CRITICAL(primask);          \
     printf(__VA_ARGS__);                      \
     MY_RTOS_EXIT_CRITICAL(primask);           \
 } while (0)
#else
#define DBG_PRINTF(...)
#endif

// ʱ��ת����
// ����תticks
#define MS_TO_TICKS(ms)         (((uint64_t)(ms) * MY_RTOS_TICK_RATE_HZ) / 1000)
// ticksת����
#define TICK_TO_MS(tick)        (((uint64_t)(tick) * 1000) / MY_RTOS_TICK_RATE_HZ)

// ǰ������
struct Task_t;
struct Mutex_t;
struct Timer_t;
struct Queue_t;


// ����״̬ö��
typedef enum {
    TASK_STATE_UNUSED = 0, // ������ƿ�δʹ��
    TASK_STATE_READY,      // �����������������
    TASK_STATE_DELAYED,    // ����������ʱ
    TASK_STATE_BLOCKED     // ������ȴ��¼�(�绥����������)������
} TaskState_t;

// ���Ķ�����
typedef struct Task_t*      TaskHandle_t;
typedef struct Mutex_t*     MutexHandle_t;
typedef struct Timer_t*     TimerHandle_t;
typedef void*               QueueHandle_t;

// ��ʱ���ص�����ָ������
typedef void (*TimerCallback_t)(TimerHandle_t timer);

// ���ĺ�
#define MY_RTOS_ENTER_CRITICAL(status_var)   do { (status_var) = __get_PRIMASK(); __disable_irq(); } while(0)
#define MY_RTOS_EXIT_CRITICAL(status_var)    do { __set_PRIMASK(status_var); } while(0)
#define MY_RTOS_YIELD()                      do { SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; __ISB(); } while(0)


/*----- System Core -----*/
/**
 * @brief ��ʼ��ϵͳ����
 */
void MyRTOS_Init(void);

/**
 * @brief �������������
 */
void Task_StartScheduler(void);

/**
 * @brief ��ȡϵͳʱ��
 * @return ��ǰϵͳʱ�� (ticks)
 */
uint64_t MyRTOS_GetTick(void);

/*----- Task Management -----*/
/**
 * @brief ��������
 * @param func ������
 * @param stack_size ����ջ��С (words, e.g., 128 for 128*4 bytes)
 * @param param �������
 * @param priority �������ȼ�
 * @return ������
 */
TaskHandle_t Task_Create(void (*func)(void *), uint16_t stack_size, void *param, uint8_t priority);

/**
 * @brief ɾ������
 * @param task_h ������. ���Ϊ NULL, ��ɾ����ǰ����.
 * @return 0 �ɹ���-1 ʧ��
 */
int Task_Delete(TaskHandle_t task_h);

/**
 * @brief ��ʱ
 * @param tick ��ʱʱ�� (ticks)
 */
void Task_Delay(uint32_t tick);

/**
 * @brief ֪ͨ����
 * @param task_h ������
 * @return 0 �ɹ���-1 ʧ��
 */
int Task_Notify(TaskHandle_t task_h);

/**
 * @brief �ȴ�����֪ͨ
 */
void Task_Wait(void);


/**
 * @brief ��ȡָ�������״̬��
 * @param task_h Ҫ��ѯ����������
 * @return ����ĵ�ǰ״̬ (TaskState_t)��
 */
TaskState_t Task_GetState(TaskHandle_t task_h);

/**
 * @brief ��ȡָ����������ȼ���
 * @param task_h Ҫ��ѯ����������
 * @return ��������ȼ���
 */
uint8_t Task_GetPriority(TaskHandle_t task_h);

/**
 * @brief ��ȡ��ǰ�������е�����ľ����
 * @return ��ǰ����ľ����
 */
TaskHandle_t Task_GetCurrentTaskHandle(void);

/*----- Queue Management -----*/
/**
 * @brief ��������
 * @param length ���г���
 * @param itemSize �������С
 * @return ���о��
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

/**
 * @brief ɾ������
 * @param delQueue Ҫɾ���Ķ��о��
 */
void Queue_Delete(QueueHandle_t delQueue);

/**
 * @brief ����з�������
 * @param queue ���о��
 * @param item  Ҫ���͵�������
 * @param block_ticks ����ʱ�� (ticks) 0��ʾ��������MY_RTOS_MAX_DELAY��ʾ���޵ȴ�
 * @return 1 ��ʾ�ɹ�, 0 ��ʾʧ��
 */
int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks);

/**
 * @brief �Ӷ��н�������
 * @param queue ���о��
 * @param buffer �������ݵĻ�����
 * @param block_ticks ����ʱ�� (ticks) 0��ʾ��������MY_RTOS_MAX_DELAY��ʾ���޵ȴ�
 * @return 1 ��ʾ�ɹ�, 0 ��ʾʧ��
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks);

/*----- Timer Management -----*/
/**
 * @brief ������ʱ��
 * @param delay ������ʱ��ticks��
 * @param period ��ʱ���ڣ�ticks��0��ʾһ���Զ�ʱ��
 * @param callback ��ʱ���ص�����
 * @param arg �ص���������
 * @return ��ʱ�����
 */
TimerHandle_t Timer_Create(uint32_t delay, uint32_t period, TimerCallback_t callback, void* arg);

/**
 * @brief ������ʱ��
 * @param timer ��ʱ�����
 * @return 0 �ɹ���-1 ʧ��
 */
int Timer_Start(TimerHandle_t timer);

/**
 * @brief ֹͣ��ʱ��
 * @param timer ��ʱ�����
 * @return 0 �ɹ���-1 ʧ��
 */
int Timer_Stop(TimerHandle_t timer);

/**
 * @brief ɾ����ʱ��
 * @param timer ��ʱ�����
 * @return 0 �ɹ���-1 ʧ��
 */
int Timer_Delete(TimerHandle_t timer);

/*----- Mutex Management -----*/
/**
 * @brief ����һ��������.
 * @return �ɹ��򷵻ػ��������, ʧ��(���ڴ治��)�򷵻� NULL.
 */
MutexHandle_t Mutex_Create(void);

/**
 * @brief ��ȡ������
 * @param mutex ���������
 */
void Mutex_Lock(MutexHandle_t mutex);

/**
 * @brief �ͷŻ�����
 * @param mutex ���������
 */
void Mutex_Unlock(MutexHandle_t mutex);


/**
 * @brief �ݹ��ȡ������
 * @param mutex ���������
 */
void Mutex_Lock_Recursive(MutexHandle_t mutex);

/**
 * @brief �ݹ��ͷŻ�����
 * @param mutex ���������
 */
void Mutex_Unlock_Recursive(MutexHandle_t mutex);


#endif // MYRTOS_H