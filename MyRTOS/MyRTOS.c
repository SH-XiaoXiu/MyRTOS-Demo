//
// Created by XiaoXiu on 8/22/2025.
//

#include "MyRTOS.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gd32f4xx.h"
#include "core_cm4.h"
#include "system_gd32f4xx.h"

//���䶨��
#ifndef SCB_CFSR_IACCVIOL_Msk
#define SCB_CFSR_IACCVIOL_Msk     (1UL << 0)
#define SCB_CFSR_DACCVIOL_Msk     (1UL << 1)
#define SCB_CFSR_MUNSTKERR_Msk    (1UL << 3)
#define SCB_CFSR_MSTKERR_Msk      (1UL << 4)
#define SCB_CFSR_MLSPERR_Msk      (1UL << 5)
#define SCB_CFSR_IBUSERR_Msk      (1UL << 8)
#define SCB_CFSR_PRECISERR_Msk    (1UL << 9)
#define SCB_CFSR_IMPRECISERR_Msk  (1UL << 10)
#define SCB_CFSR_UNSTKERR_Msk     (1UL << 11)
#define SCB_CFSR_STKERR_Msk       (1UL << 12)
#define SCB_CFSR_LSPERR_Msk       (1UL << 13)
#define SCB_CFSR_UNDEFINSTR_Msk   (1UL << 16)
#define SCB_CFSR_INVSTATE_Msk     (1UL << 17)
#define SCB_CFSR_INVPC_Msk        (1UL << 18)
#endif

//====================== ��̬�ڴ���� ======================

#define RTOS_MEMORY_POOL_SIZE (16 * 1024)
#define HEAP_BYTE_ALIGNMENT   8

/* �ڴ���ͷ���ṹ�����ڹ��������ڴ������ */
typedef struct BlockLink_t {
    struct BlockLink_t *pxNextFreeBlock; /* ָ����������һ�������ڴ�� */
    size_t xBlockSize; /* ��ǰ�ڴ��Ĵ�С(����ͷ��), ���λ���������� */
} BlockLink_t;

/* �ڴ��ͷ���ṹ�Ĵ�С (�Ѷ���) */
static const size_t xHeapStructSize = (sizeof(BlockLink_t) + (HEAP_BYTE_ALIGNMENT - 1)) & ~(
                                          (size_t) HEAP_BYTE_ALIGNMENT - 1);
/* ��С�ڴ���С��һ������������ɷ��Ѻ������ͷ�� */
#define HEAP_MINIMUM_BLOCK_SIZE    (xHeapStructSize * 2)

/* ��̬�ڴ�� */
static uint8_t rtos_memory_pool[RTOS_MEMORY_POOL_SIZE] __attribute__((aligned(HEAP_BYTE_ALIGNMENT)));
/* �����������ʼ�ͽ������ */
static BlockLink_t xStart, *pxEnd = NULL;
/* ʣ������ڴ��С */
static size_t xFreeBytesRemaining = 0U;
/* ���ڱ���ڴ���Ƿ��ѱ�����ı�־λ */
static size_t xBlockAllocatedBit = 0;

/* ��ʼ�����ڴ� (�����״η���ʱ����) */
static void prvHeapInit(void) {
    BlockLink_t *pxFirstFreeBlock;
    uint8_t *pucAlignedHeap;
    size_t uxAddress = (size_t) rtos_memory_pool;
    size_t xTotalHeapSize = RTOS_MEMORY_POOL_SIZE;

    if ((uxAddress & (HEAP_BYTE_ALIGNMENT - 1)) != 0) {
        uxAddress += (HEAP_BYTE_ALIGNMENT - (uxAddress & (HEAP_BYTE_ALIGNMENT - 1)));
        xTotalHeapSize -= uxAddress - (size_t) rtos_memory_pool;
    }
    pucAlignedHeap = (uint8_t *) uxAddress;

    xStart.pxNextFreeBlock = (BlockLink_t *) pucAlignedHeap;
    xStart.xBlockSize = (size_t) 0;

    uxAddress = ((size_t) pucAlignedHeap) + xTotalHeapSize - xHeapStructSize;
    pxEnd = (BlockLink_t *) uxAddress;
    pxEnd->xBlockSize = 0;
    pxEnd->pxNextFreeBlock = NULL;

    pxFirstFreeBlock = (BlockLink_t *) pucAlignedHeap;
    pxFirstFreeBlock->xBlockSize = uxAddress - (size_t) pxFirstFreeBlock;
    pxFirstFreeBlock->pxNextFreeBlock = pxEnd;

    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
    xBlockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * 8) - 1);
}

/* ��һ���ڴ����뵽���������У������������ڿ��п�ĺϲ� */
static void prvInsertBlockIntoFreeList(BlockLink_t *pxBlockToInsert) {
    BlockLink_t *pxIterator;
    uint8_t *puc;

    /* ���������ҵ����Բ������ͷſ��λ�� (����ַ����) */
    for (pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert;
         pxIterator = pxIterator->pxNextFreeBlock) {
        /* ʲô����������ֻ���ҵ�λ�� */
    }

    /* �����¿��Ƿ��������ָ��Ŀ������������� */
    puc = (uint8_t *) pxIterator;
    if ((puc + pxIterator->xBlockSize) == (uint8_t *) pxBlockToInsert) {
        /* ���ڣ��ϲ����� */
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        /* �ϲ����¿���ǵ�����ָ��Ŀ��� */
        pxBlockToInsert = pxIterator;
    } else {
        /* �����ڣ����¿����ӵ����������� */
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* �����¿��Ƿ������Ŀ����� */
    puc = (uint8_t *) pxBlockToInsert;
    if ((puc + pxBlockToInsert->xBlockSize) == (uint8_t *) pxIterator->pxNextFreeBlock) {
        if (pxIterator->pxNextFreeBlock != pxEnd) {
            /* ���ڣ��ϲ����� */
            pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        }
    }

    /* �����һ��û�кϲ�����ô����������һ���ڵ���Ҫָ���¿� */
    if (pxIterator != pxBlockToInsert) {
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
}

/* ��̬�ڴ���亯�� */
void *rtos_malloc(size_t xWantedSize) {
    BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
    void *pvReturn = NULL;
    uint32_t primask_status;

    MY_RTOS_ENTER_CRITICAL(primask_status); {
        if (pxEnd == NULL) {
            prvHeapInit();
        }

        if ((xWantedSize > 0) && ((xWantedSize & xBlockAllocatedBit) == 0)) {
            size_t xTotalSize = xHeapStructSize + xWantedSize;
            if ((xTotalSize & (HEAP_BYTE_ALIGNMENT - 1)) != 0) {
                xTotalSize += (HEAP_BYTE_ALIGNMENT - (xTotalSize & (HEAP_BYTE_ALIGNMENT - 1)));
            }

            if (xTotalSize <= xFreeBytesRemaining) {
                pxPreviousBlock = &xStart;
                pxBlock = xStart.pxNextFreeBlock;
                while ((pxBlock->xBlockSize < xTotalSize) && (pxBlock->pxNextFreeBlock != NULL)) {
                    pxPreviousBlock = pxBlock;
                    pxBlock = pxBlock->pxNextFreeBlock;
                }

                if (pxBlock != pxEnd) {
                    pvReturn = (void *) (((uint8_t *) pxBlock) + xHeapStructSize);
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    if ((pxBlock->xBlockSize - xTotalSize) > HEAP_MINIMUM_BLOCK_SIZE) {
                        pxNewBlockLink = (BlockLink_t *) (((uint8_t *) pxBlock) + xTotalSize);
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xTotalSize;
                        pxBlock->xBlockSize = xTotalSize;
                        prvInsertBlockIntoFreeList(pxNewBlockLink);
                    }

                    xFreeBytesRemaining -= pxBlock->xBlockSize;
                    pxBlock->xBlockSize |= xBlockAllocatedBit;
                    pxBlock->pxNextFreeBlock = NULL;
                }
            }
        }
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);

    return pvReturn;
}

/* ��̬�ڴ��ͷź��� */
void rtos_free(void *pv) {
    if (pv == NULL) return;

    uint8_t *puc = (uint8_t *) pv;
    BlockLink_t *pxLink;
    uint32_t primask_status;

    puc -= xHeapStructSize;
    pxLink = (BlockLink_t *) puc;

    if (((pxLink->xBlockSize & xBlockAllocatedBit) != 0) && (pxLink->pxNextFreeBlock == NULL)) {
        pxLink->xBlockSize &= ~xBlockAllocatedBit;
        MY_RTOS_ENTER_CRITICAL(primask_status); {
            xFreeBytesRemaining += pxLink->xBlockSize;
            prvInsertBlockIntoFreeList(pxLink);
        }
        MY_RTOS_EXIT_CRITICAL(primask_status);
    }
}

//================================================================================

// ȫ�ֱ���
static Task_t *taskListHead = NULL;
static Task_t *currentTask = NULL;
static Task_t *idleTask = NULL;
static uint32_t nextTaskId = 0;


void MyRTOS_Init(void) {
    taskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    nextTaskId = 0;
    DBG_PRINTF("MyRTOS Initialized. Task list cleared and memory manager reset.\n");
}

//================= Task ================
#define SIZEOF_TASK_T sizeof(Task_t)

//ͨ��ID��������
static Task_t *find_task_by_id(uint32_t task_id) {
    Task_t *p = taskListHead;
    while (p != NULL) {
        if (p->taskId == task_id) return p;
        p = p->next;
    }
    return NULL;
}

void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // ����͹���ģʽ���ȴ��ж�
    }
}

Task_t *Task_Create(void (*func)(void *), void *param) {
    //Ϊ������ƿ� TCB �����ڴ�
    Task_t *t = rtos_malloc(sizeof(Task_t));
    if (t == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for TCB.\n");
        return NULL;
    }

    //Ϊ����ջ�����ڴ�
    uint32_t stack_size_bytes = STACK_SIZE * sizeof(uint32_t);
    uint32_t *stack = rtos_malloc(stack_size_bytes);
    if (stack == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for stack.\n");
        rtos_free(t); // <<< ADDED: ���ջ����ʧ�ܣ��ع�TCB�ķ���
        return NULL;
    }

    // ��ʼ��TCB��Ա
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    t->state = TASK_STATE_READY;
    t->taskId = nextTaskId++; // ���䲢����ID
    t->stack_base = stack; // ����ջ����ַ�����ں����ͷ�
    t->next = NULL;

    uint32_t *sp = stack + STACK_SIZE;
    sp = (uint32_t *) (((uintptr_t) sp) & ~0x7u); // 8 �ֽڶ���

    /* ��дӲ���Զ�����֡��Ӳ���� EXC_RETURN ʱ������ */
    sp -= 8;
    sp[0] = (uint32_t) param; // R0
    sp[1] = 0x01010101; // R1
    sp[2] = 0x02020202; // R2
    sp[3] = 0x03030303; // R3
    sp[4] = 0x12121212; // R12
    sp[5] = 0x00000000; // LR (���񷵻�)
    sp[6] = ((uint32_t) func) | 1u; // PC (Thumb)
    sp[7] = 0x01000000; // xPSR (T bit)

    /* ��д�����������PendSV/SVC �ָ� R4-R11�� */
    sp -= 8;
    sp[0] = 0x04040404;
    sp[1] = 0x05050505;
    sp[2] = 0x06060606;
    sp[3] = 0x07070707;
    sp[4] = 0x08080808;
    sp[5] = 0x09090909;
    sp[6] = 0x0A0A0A0A;
    sp[7] = 0x0B0B0B0B;
    t->sp = sp;

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (taskListHead == NULL) {
        taskListHead = t;
    } else {
        Task_t *p = taskListHead;
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = t;
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);

    DBG_PRINTF("Task %lu created. Stack top: %p, Initial SP: %p\n", t->taskId, &stack[STACK_SIZE - 1], t->sp);
    return t; // �����ɹ�, ����������
}

// ɾ������
int Task_Delete(const Task_t *task_h) {
    // ������ɾ�� NULL ������������
    if (task_h == NULL || task_h == idleTask) {
        return -1;
    }
    //��Ҫ�޸����� TCB �����ݣ�������Ҫһ���� const ��ָ��
    Task_t *task_to_delete = (Task_t *) task_h;
    uint32_t primask_status;
    int trigger_yield = 0; // �Ƿ���Ҫ�ں���ĩβ�������ȵı�־
    MY_RTOS_ENTER_CRITICAL(primask_status);
    //�Զ��ͷ�������е����л���������ֹ����
    Mutex_t *p_mutex = task_to_delete->held_mutexes_head;
    while (p_mutex != NULL) {
        Mutex_t *next_mutex = p_mutex->next_held_mutex;
        // �ֶ�����
        p_mutex->locked = 0;
        p_mutex->owner = (uint32_t) -1;
        p_mutex->owner_tcb = NULL;
        p_mutex->next_held_mutex = NULL;
        // ��������������ڵȴ������������
        if (p_mutex->waiting_mask != 0) {
            Task_t *p_task = taskListHead;
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (p_mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        p_task->state = TASK_STATE_READY;
                        trigger_yield = 1; // ���������񣬽��е���
                    }
                }
                p_task = p_task->next;
            }
        }
        p_mutex = next_mutex;
    }
    //�������ȫ�������������Ƴ�
    Task_t *prev = NULL;
    Task_t *curr = taskListHead;
    while (curr != NULL && curr != task_to_delete) {
        prev = curr;
        curr = curr->next;
    }
    // ��������������� ��ֱ�ӷ���
    if (curr == NULL) {
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return -2;
    }
    if (prev == NULL) {
        taskListHead = curr->next;
    } else {
        prev->next = curr->next;
    }
    //�ͷ�����ռ�õ��ڴ� (ջ��TCB)
    rtos_free(curr->stack_base);
    rtos_free(curr);
    //�������
    // ���ɾ�����ǵ�ǰ�������е����񣬱���������������
    if (curr == currentTask) {
        currentTask = NULL; // ǿ�Ƶ�������ͷ��ʼѰ����һ������
        trigger_yield = 1;
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
    // ���ٽ���֮��ִ�� yield
    if (trigger_yield) {
        MY_RTOS_YIELD();
    }
    DBG_PRINTF("Task %lu deleted and memory reclaimed.\n", task_h->taskId);
    return 0;
}

void Task_Delay(uint32_t tick) {
    if (tick == 0) return;
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    currentTask->delay = tick;
    currentTask->state = TASK_STATE_DELAYED;

    MY_RTOS_EXIT_CRITICAL(primask_status);
    MY_RTOS_YIELD();
    __ISB();
}

__attribute__((naked)) void Start_First_Task(void) {
    __asm volatile (
        "ldr r0, =currentTask      \n"
        "ldr r0, [r0]              \n" // r0 = currentTask
        "ldr r0, [r0]              \n" // r0 = currentTask->sp  (ָ����������� R4-R11)

        "ldmia r0!, {r4-r11}       \n" // ���� R4-R11��r0 ָ��Ӳ��ջ֡
        "msr psp, r0               \n" // PSP = Ӳ��ջ֡��ʼ
        "isb                       \n"

        "movs r0, #2               \n" // Thread+PSP
        "msr control, r0           \n"
        "isb                       \n"

        "ldr r0, =0xFFFFFFFD       \n" // EXC_RETURN: Thread, PSP, Return to Thumb
        "mov lr, r0                \n"
        "bx lr                     \n" // ֻ�� bx lr
    );
}

void Task_StartScheduler(void) {
    // ������������
    idleTask = Task_Create(MyRTOS_Idle_Task, NULL);
    if (idleTask == NULL) {
        DBG_PRINTF("Error: Failed to create Idle Task!\n");
        while (1);
    }
    DBG_PRINTF("Idle task created successfully at address: %p\n", idleTask);
    DBG_PRINTF("Idle task initial SP: %p\n", idleTask->sp);
    SCB->VTOR = (uint32_t) 0x08000000;
    DBG_PRINTF("Starting scheduler...\n");

    //����Ҫ����PenSV��SysTick���ж����ȼ� ��Ȼ���
    NVIC_SetPriority(PendSV_IRQn, 0xFF); // ������ȼ�
    NVIC_SetPriority(SysTick_IRQn, 0x00); // ��PendSV��һ����
    if (SysTick_Config(SystemCoreClock / 1000)) {
        // 1ms �Ӳ����Լ�������
        DBG_PRINTF("Error: SysTick_Config failed\n");
        while (1);
    }
    DBG_PRINTF("Idle task sp: %p\n", idleTask->sp);
    // ���õ�ǰ����ΪIdle
    currentTask = idleTask;
    /* ȷ�� MSP ָ���������еĳ�ʼջ���ο� FreeRTOS ������ */
    __asm volatile(
        "ldr r0, =0xE000ED08\n"
        "ldr r0, [r0]\n"
        "ldr r0, [r0]\n"
        "msr msp, r0\n"
    );
    /* ���� SVC ���쳣������״������Ļָ� */
    __asm volatile("svc 0");
    for (;;); /* ���᷵�� */
    // �����ϲ���ִ�е�����Ĺ� ��Ȼ���ų�ʲô�������ߵ����ڴ淢�����ط�ת. ��߽������Ʋ���������Ӳ��ʹ��ECC�ڴ�
    DBG_PRINTF("Error: Scheduler returned!\n");
    while (1);
}

// �������������
void *schedule_next_task(void) {
    Task_t *next_task_to_run = NULL;
    Task_t *start_node;
    // �����ǰ����������к�̽ڵ㣬��Ӻ�̽ڵ㿪ʼ������ʵ��O(1)��ѯ��
    if (currentTask && currentTask->next) {
        start_node = currentTask->next;
    } else {
        start_node = taskListHead;
    }

    Task_t *p = start_node;
    while (p != NULL) {
        if (p->state == TASK_STATE_READY) {
            next_task_to_run = p;
            goto found_task; // �ҵ���������
        }
        p = p->next;
    }
    p = taskListHead;
    while (p != start_node) {
        if (p->state == TASK_STATE_READY) {
            next_task_to_run = p;
            goto found_task;
        }
        p = p->next;
    }

found_task:
    if (next_task_to_run != NULL) {
        currentTask = next_task_to_run;
    } else {
        currentTask = idleTask;
    }

    if (currentTask == NULL) {
        return NULL;
    }

    return currentTask->sp;
}

//================= Task ================

//=================== �ź���==================
int Task_Notify(uint32_t task_id) {
    Task_t *task_to_notify = find_task_by_id(task_id);
    if (task_to_notify == NULL) {
        return -1; // ��ЧID
    }

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    if (task_to_notify->is_waiting_notification && task_to_notify->state == TASK_STATE_BLOCKED) {
        task_to_notify->is_waiting_notification = 0;
        task_to_notify->state = TASK_STATE_READY; // ����
        MY_RTOS_YIELD();
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
    return 0;
}

void Task_Wait(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    currentTask->is_waiting_notification = 1;
    currentTask->state = TASK_STATE_BLOCKED; // ����Ϊ����
    MY_RTOS_EXIT_CRITICAL(primask_status);
    MY_RTOS_YIELD();
    __ISB();
}

//=================== �ź���==================


//============== ������ =============
void Mutex_Init(Mutex_t *mutex) {
    mutex->locked = 0;
    mutex->owner = (uint32_t) -1;
    mutex->waiting_mask = 0;
    mutex->owner_tcb = NULL;
    mutex->next_held_mutex = NULL;
}

void Mutex_Lock(Mutex_t *mutex) {
    uint32_t primask_status;

    while (1) {
        // ʹ������ѭ���Լ��߼�
        MY_RTOS_ENTER_CRITICAL(primask_status);
        if (!mutex->locked) {
            // ��ȡ���ɹ�
            mutex->locked = 1;
            mutex->owner = currentTask->taskId;
            mutex->owner_tcb = currentTask; // ��¼������TCB
            // --- ������ӵ�����ĳ�������ͷ��
            mutex->next_held_mutex = currentTask->held_mutexes_head;
            currentTask->held_mutexes_head = mutex;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return; // �ɹ���ȡ������
        } else {
            // ����ռ�ã�����ȴ�
            if (currentTask->taskId < 32) {
                mutex->waiting_mask |= (1 << currentTask->taskId);
            }
            currentTask->state = TASK_STATE_BLOCKED;
            // �������ȣ��ó�CPU
            MY_RTOS_YIELD();
            MY_RTOS_EXIT_CRITICAL(primask_status);
        }
    }
}

void Mutex_Unlock(Mutex_t *mutex) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner == currentTask->taskId) {
        // --- ������ĳ����������Ƴ����� ---
        //���򵥵ĵ�������
        if (currentTask->held_mutexes_head == mutex) {
            // �����ͷ�ڵ�
            currentTask->held_mutexes_head = mutex->next_held_mutex;
        } else {
            Mutex_t *p = currentTask->held_mutexes_head;
            while (p != NULL && p->next_held_mutex != mutex) {
                p = p->next_held_mutex;
            }
            if (p != NULL) {
                p->next_held_mutex = mutex->next_held_mutex;
            }
        }
        mutex->next_held_mutex = NULL; // ����ָ��
        // �ͷ���
        mutex->locked = 0;
        mutex->owner = (uint32_t) -1;
        mutex->owner_tcb = NULL;
        // ���ѵȴ�������
        if (mutex->waiting_mask != 0) {
            Task_t *p_task = taskListHead;
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        p_task->state = TASK_STATE_READY;
                    }
                }
                p_task = p_task->next;
            }
            // ����������������ý���һ�ε���
            MY_RTOS_YIELD();
        }
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

//============== ������ =============


//=========== Handler ============

void SysTick_Handler(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    Task_t *p = taskListHead;
    while (p != NULL) {
        if (p->state == TASK_STATE_DELAYED) {
            if (p->delay > 0) {
                p->delay--;
            }
            if (p->delay == 0) {
                p->state = TASK_STATE_READY;
            }
        }
        p = p->next;
    }
    MY_RTOS_YIELD();
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        "ldr r2, =currentTask      \n"
        "ldr r3, [r2]              \n"
        "cbz r3, 1f                \n" // currentTask==NULL? �״Σ�������
        "mrs r0, psp               \n"
        "stmdb r0!, {r4-r11}       \n"
        "str  r0, [r3]             \n" // currentTask->sp = ��ջ��
        "1: \n"
        "bl  schedule_next_task    \n" // r0 = next->sp��ָ������������ײ���
        "ldmia r0!, {r4-r11}       \n"
        "msr psp, r0               \n"
        "mov r0, #0xFFFFFFFD       \n"
        "mov lr, r0                \n"
        "bx  lr                    \n"
    );
}

void HardFault_Handler(void) {
    __disable_irq();

    uint32_t stacked_pc = 0;
    uint32_t sp = 0;
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;

    /* �ж��쳣����ʹ������ջ (EXC_RETURN bit2) */
    register uint32_t lr __asm("lr");

    if (lr & 0x4) {
        // ʹ�� PSP
        sp = __get_PSP();
    } else {
        // ʹ�� MSP
        sp = __get_MSP();
    }

    stacked_pc = ((uint32_t *) sp)[6]; // PC ����Ӳ���Զ�����֡�ĵ� 6 ��λ��

    DBG_PRINTF("\n!!! Hard Fault !!!\n");
    DBG_PRINTF("CFSR: 0x%08lX, HFSR: 0x%08lX\n", cfsr, hfsr);
    DBG_PRINTF("LR: 0x%08lX, SP: 0x%08lX, Stacked PC: 0x%08lX\n", lr, sp, stacked_pc);

    if (cfsr & SCB_CFSR_IACCVIOL_Msk) DBG_PRINTF("Fault: Instruction Access Violation\n");
    if (cfsr & SCB_CFSR_DACCVIOL_Msk) DBG_PRINTF("Fault: Data Access Violation\n");
    if (cfsr & SCB_CFSR_MUNSTKERR_Msk) DBG_PRINTF("Fault: Unstacking Error\n");
    if (cfsr & SCB_CFSR_MSTKERR_Msk) DBG_PRINTF("Fault: Stacking Error\n");
    if (cfsr & SCB_CFSR_INVSTATE_Msk) DBG_PRINTF("Fault: Invalid State\n");
    if (cfsr & SCB_CFSR_UNDEFINSTR_Msk) DBG_PRINTF("Fault: Undefined Instruction\n");
    if (cfsr & SCB_CFSR_IBUSERR_Msk) DBG_PRINTF("Fault: Instruction Bus Error\n");
    if (cfsr & SCB_CFSR_PRECISERR_Msk) DBG_PRINTF("Fault: Precise Data Bus Error\n");

    while (1);
}


__attribute__((naked)) void SVC_Handler(void) {
    __asm volatile (
        "ldr r1, =currentTask      \n"
        "ldr r1, [r1]              \n" /* r1 = currentTask */
        "ldr r0, [r1]              \n" /* r0 = currentTask->sp (ָ������������ײ�) */

        "ldmia r0!, {r4-r11}       \n" /* �ָ� R4-R11��r0 -> Ӳ��֡ */
        "msr psp, r0               \n" /* PSP ָ��Ӳ��֡ */
        "isb                       \n"

        "movs r0, #2               \n" /* Thread mode, use PSP */
        "msr control, r0           \n"
        "isb                       \n"

        "ldr r0, =0xFFFFFFFD       \n" /* EXC_RETURN: thread, return using PSP */
        "mov lr, r0                \n"
        "bx lr                     \n"
    );
}

//=========== Handler ============
