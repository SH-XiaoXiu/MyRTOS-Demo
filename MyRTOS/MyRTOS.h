#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>

#define MAX_TASKS   8
#define STACK_SIZE  256
#define IDLE_TASK_ID (MAX_TASKS-1)


// �����������
#define DEBUG_PRINT 0  // ����Ϊ1�����������,0�ر�
//�����������
#if DEBUG_PRINT
#include <stdio.h>
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

// ��������״̬
typedef enum {
    TASK_STATE_UNUSED, // δʹ��
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
    struct Task_t* owner_tcb;
    struct Mutex_t* next_held_mutex;
} Mutex_t;

// ������ƿ� (TCB)
//Ϊ�˱��ֻ�����ļ����� sp �����ǽṹ��ĵ�һ����Ա
typedef struct Task_t {
    void *sp; // ջָ�� (Stack Pointer)
    void (*func)(void *); // ������ָ��
    void *param; // �������
    volatile uint32_t delay; // ������ʱ������
    volatile TaskState_t state; // ����״̬
    uint32_t notification; // ����ֵ֪ͨ
    uint8_t is_waiting_notification; // �Ƿ��ڵȴ�֪ͨ�ı�־
    uint32_t taskId; // ����ID
    uint32_t *stack_base; // ջ�Ļ���ַ�������ͷ��ڴ�
    struct Task_t *next; // ָ����һ�������ָ��
    Mutex_t* held_mutexes_head;
} Task_t;





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

Task_t *Task_Create(void (*func)(void *), void *param);

int Task_Delete(const Task_t *task_h);

void Task_StartScheduler(void);

void Task_Delay(uint32_t tick);

int Task_Notify(uint32_t task_id);

void Task_Wait(void);

void Mutex_Init(Mutex_t *mutex);

void Mutex_Lock(Mutex_t *mutex);

void Mutex_Unlock(Mutex_t *mutex);
#endif //MYRTOS_H
