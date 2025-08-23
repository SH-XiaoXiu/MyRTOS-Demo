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

// ������ƿ� (TCB)
//Ϊ�˱��ֻ�����ļ����� sp �����ǽṹ��ĵ�һ����Ա
typedef struct Task_t {
    void *sp;                        // ջָ�� (Stack Pointer)
    void (*func)(void *);            // ������ָ��
    void *param;                     // �������
    volatile uint32_t delay;         // ������ʱ������
    volatile TaskState_t state;      // ����״̬
    uint32_t notification;           // ����ֵ֪ͨ
    uint8_t is_waiting_notification; // �Ƿ��ڵȴ�֪ͨ�ı�־
    uint32_t taskId;                 // ����ID
    uint32_t *stack_base;            // ջ�Ļ���ַ�������ͷ��ڴ�
    struct Task_t *next;             // ָ����һ�������ָ��
} Task_t;

// �������ṹ��
typedef struct {
    volatile int locked; // ��״̬
    volatile uint32_t owner; // ��ǰ������������ID
    volatile uint32_t waiting_mask; // �ȴ��������������� (���� MAX_TASKS <= 32)
} Mutex_t;

Task_t* Task_Create(void (*func)(void *), void *param);

int Task_Delete(const Task_t *task_h);

void Task_StartScheduler(void);

void Task_Delay(uint32_t tick);

int Task_Notify(uint32_t task_id);

void Task_Wait(void);

void Mutex_Init(Mutex_t *mutex);

void Mutex_Lock(Mutex_t *mutex);

void Mutex_Unlock(Mutex_t *mutex);
#endif //MYRTOS_H
