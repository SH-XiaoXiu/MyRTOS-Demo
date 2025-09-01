//
// Created by XiaoXiu on 8/30/2025.
//

#ifndef MYRTOS_KERNEL_PRIVATE_H
#define MYRTOS_KERNEL_PRIVATE_H

#include "MyRTOS.h"
#include "MyRTOS_Port.h"

/*===========================================================================*
 *                      �ڲ����ݽṹ����                                     *
 *===========================================================================*/

// --- �ڴ����ṹ ---
/**
 * @brief �ڴ�����ӽṹ��
 */
typedef struct BlockLink_t {
    struct BlockLink_t *nextFreeBlock; // ָ����һ�������ڴ��
    size_t blockSize; // ��ǰ�ڴ���С
} BlockLink_t;


// --- �ں˺��Ķ���ṹ ---
/**
 * @brief �¼��б�ṹ��
 */
typedef struct EventList_t {
    volatile TaskHandle_t head; // �¼��б�ͷ�ڵ�
} EventList_t;

/**
 * @brief �������ṹ��
 */
typedef struct Mutex_t {
    volatile int locked; // ��״̬���
    struct Task_t *owner_tcb; // ӵ�иû�����������TCB
    struct Mutex_t *next_held_mutex; // ��һ�����еĻ�����
    EventList_t eventList; // �ȴ��û������������¼��б�
    volatile uint32_t recursion_count; // �ݹ���������
} Mutex_t;

/**
 * @brief ������ƿ�ṹ��
 */
typedef struct Task_t {
    StackType_t *sp; // ����ջָ��

    void (*func)(void *); // ������ָ��

    void *param; // ����������
    uint64_t delay; // ������ʱ����
    volatile uint32_t notification; // ����ֵ֪ͨ
    volatile uint8_t is_waiting_notification; // �Ƿ����ڵȴ�֪ͨ
    volatile TaskState_t state; // ����״̬
    uint32_t taskId; // ����ID
    StackType_t *stack_base; // ����ջ����ַ
    uint8_t priority; // �������ȼ�
    uint8_t basePriority; // ����������ȼ�
    struct Task_t *pNextTask; // ָ����һ������(��������)
    struct Task_t *pNextGeneric; // ͨ��������һ�ڵ�ָ��
    struct Task_t *pPrevGeneric; // ͨ��������һ�ڵ�ָ��
    struct Task_t *pNextEvent; // �¼�������һ�ڵ�ָ��
    EventList_t *pEventList; // ���������¼��б�
    Mutex_t *held_mutexes_head; // ������еĻ���������ͷ
    void *eventData; // �¼��������
    const char *taskName; // ��������
    uint16_t stackSize_words; // ����ջ��С(��)
} Task_t;

// TCB��stack_base�ֶε�ƫ����
#define TCB_OFFSET_STACK_BASE   offsetof(Task_t, stack_base)

/**
 * @brief ���нṹ��
 */
typedef struct Queue_t {
    uint8_t *storage; // ���д洢����
    uint32_t length; // ���г���
    uint32_t itemSize; // �������С
    volatile uint32_t waitingCount; // �ȴ�����
    uint8_t *writePtr; // дָ��
    uint8_t *readPtr; // ��ָ��
    EventList_t sendEventList; // �����¼��б�
    EventList_t receiveEventList; // �����¼��б�
} Queue_t;

/**
 * @brief �ź����ṹ��
 */
typedef struct Semaphore_t {
    volatile uint32_t count; // �ź�������
    uint32_t maxCount; // �ź���������
    EventList_t eventList; // �ȴ����ź����������¼��б�
} Semaphore_t;


/*===========================================================================*
 *                      �ڲ�ȫ�ֱ�������                                     *
 *===========================================================================*/
extern TaskHandle_t allTaskListHead; // ������������ͷ
extern size_t freeBytesRemaining; // ʣ������ڴ��ֽ���

#endif // MYRTOS_KERNEL_PRIVATE_H


