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
typedef struct {
    void *sp; // ����ջ��ָ�� (�����ǵ�һ����Ա��������)
    TaskState_t state; // ����ǰ״̬
    void (*func)(void *); // ������ָ��
    void *param; // ����������
    uint32_t delay; // ��ʱ������
    uint32_t notification; //ֵ֪ͨ -�����ֶ�
    int is_waiting_notification; // ����Ƿ��ڵȴ�֪ͨ
} Task_t;

// �������ṹ��
typedef struct {
    volatile int locked; // ��״̬
    volatile uint32_t owner; // ��ǰ������������ID
    volatile uint32_t waiting_mask; // �ȴ��������������� (���� MAX_TASKS <= 32)
} Mutex_t;

int Task_Create(uint32_t taskId, void (*func)(void *), void *param);

void Task_StartScheduler(void);

void Task_Delay(uint32_t tick);

int Task_Notify(uint32_t task_id);

void Task_Wait(void);

void Mutex_Init(Mutex_t *mutex);

void Mutex_Lock(Mutex_t *mutex);

void Mutex_Unlock(Mutex_t *mutex);
#endif //MYRTOS_H
