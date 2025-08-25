#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>

#define MAX_TASKS   8
#define STACK_SIZE  256
#define IDLE_TASK_ID (MAX_TASKS-1)
#define MY_RTOS_MAX_PRIORITIES    (16)

// �����������
#define DEBUG_PRINT 1  // ����Ϊ1�����������,0�ر�
//�����������
#if DEBUG_PRINT
#include <stdio.h>
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

// ��������״̬
typedef enum {
    TASK_STATE_UNUSED = 0, // δʹ��
    TASK_STATE_READY, //��������������
    TASK_STATE_DELAYED, //������ʱ
    TASK_STATE_BLOCKED // ������ȴ���Դ���绥������֪ͨ��������
} TaskState_t;


struct Task_t;

// �������ṹ��
typedef struct Mutex_t {
    volatile int locked;
    volatile uint32_t owner;
    volatile uint32_t waiting_mask;
    struct Task_t *owner_tcb;
    struct Mutex_t *next_held_mutex;
} Mutex_t;

// ������ƿ� (TCB)
//Ϊ�˱��ֻ�����ļ����� sp �����ǽṹ��ĵ�һ����Ա
typedef struct Task_t {
    uint32_t *sp;
    void (*func)(void *);     // ������
    void *param;              // �������
    uint32_t delay;           // ��ʱ
    volatile uint32_t notification;
    volatile uint8_t is_waiting_notification;
    volatile TaskState_t state; // ����״̬
    uint32_t taskId;          // ����ID
    uint32_t *stack_base;     // ջ����ַ,����free
    uint8_t priority;         //�������ȼ�
    struct Task_t *pNextTask; //����������������
    struct Task_t *pNextReady; //���ھ�������ʱ����
    struct Task_t *pPrevReady; //����˫������,����ɾ�� O(1)���Ӷ�
    Mutex_t *held_mutexes_head;
    void *eventObject;         // ָ�����ڵȴ����ں˶���
    void *eventData;           // ���ڴ������¼���ص�����ָ�� (����Ϣ��Դ/Ŀ�ĵ�ַ)
    struct Task_t *pNextEvent;  // ���ڹ����ں˶���ĵȴ���������
} Task_t;


typedef void* QueueHandle_t;

typedef struct Queue_t {
    uint8_t *storage;           // ָ����д洢����ָ��
    uint32_t length;            // ������������ɵ���Ϣ��
    uint32_t itemSize;          // ÿ����Ϣ�Ĵ�С
    volatile uint32_t waitingCount; // ��ǰ�����е���Ϣ��
    uint8_t *writePtr;          // ��һ��Ҫд�����ݵ�λ��
    uint8_t *readPtr;           // ��һ��Ҫ��ȡ���ݵ�λ��
    // �ȴ��б� (�����������ȼ�����)
    Task_t *sendWaitList;
    Task_t *receiveWaitList;
} Queue_t;

/**
 * @brief ����һ����Ϣ����
 * @param length �����ܹ����ɵ������Ϣ����
 * @param itemSize ÿ����Ϣ�Ĵ�С (�ֽ�)
 * @return �ɹ����ض��о����ʧ�ܷ��� NULL
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

/**
 * @brief ɾ��һ����Ϣ����
 * @param delQueue Ҫɾ���Ķ��о��
 */
void Queue_Delete(QueueHandle_t delQueue);

/**
 * @brief ����з���һ����Ϣ
 * @param queue ���о��
 * @param item ָ��Ҫ���͵���Ϣ��ָ��
 * @param block 0: ������, 1: ��������ֱ�����ͳɹ�
 * @return 1 ��ʾ�ɹ�, 0 ��ʾʧ�� (�������Ҳ�����)
 */
int Queue_Send(QueueHandle_t queue, const void *item, int block);

/**
 * @brief �Ӷ��н���һ����Ϣ
 * @param queue ���о��
 * @param buffer ���ڴ�Ž��յ�����Ϣ�Ļ�����ָ��
 * @param block 0: ������, 1: ��������ֱ�����յ���Ϣ
 * @return 1 ��ʾ�ɹ�, 0 ��ʾʧ�� (���п��Ҳ�����)
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, int block);


/**
 * @brief �����ٽ���
 *
 * �ú�ᱣ�浱ǰ���ж�״̬��PRIMASK�Ĵ�������Ȼ��������п������жϡ�
 * ������ MY_RTOS_EXIT_CRITICAL �ɶ�ʹ�á�
 *
 * @param status_var һ�� uint32_t ���͵ľֲ����������ڱ����ж�״̬��
 */
#define MY_RTOS_ENTER_CRITICAL(status_var)   \
do {                                     \
(status_var) = __get_PRIMASK();      \
__disable_irq();                     \
} while(0)

/**
 * @brief �˳��ٽ���
 *
 * �ú��ָ��� MY_RTOS_ENTER_CRITICAL ������ж�״̬��
 *
 * @param status_var ֮ǰ���ڱ����ж�״̬��ͬһ��������
 */
#define MY_RTOS_EXIT_CRITICAL(status_var)       \
do {                                            \
__set_PRIMASK(status_var);                      \
} while(0)


/**
 * @brief �ֶ������������
 *
 * �ú����� PendSV �жϣ�������������ʵ���ʱ��ͨ�������������жϴ�����Ϻ�
 * ����һ�������������л�, j���������ò�
 */
#define MY_RTOS_YIELD()                      \
do {                                     \
SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; \
__ISB();                             \
} while(0)

void MyRTOS_Init(void);

Task_t *Task_Create(void (*func)(void *), void *param, uint8_t priority) ;

int Task_Delete(const Task_t *task_h);

void Task_StartScheduler(void);

void Task_Delay(uint32_t tick);

int Task_Notify(uint32_t task_id);

void Task_Wait(void);

void Mutex_Init(Mutex_t *mutex);

void Mutex_Lock(Mutex_t *mutex);

void Mutex_Unlock(Mutex_t *mutex);
#endif //MYRTOS_H
