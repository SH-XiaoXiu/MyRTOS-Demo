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
    struct BlockLink_t *nextFreeBlock; /* ָ����������һ�������ڴ�� */
    size_t blockSize; /* ��ǰ�ڴ��Ĵ�С(����ͷ��), ���λ���������� */
} BlockLink_t;

/* �ڴ��ͷ���ṹ�Ĵ�С (�Ѷ���) */
static const size_t heapStructSize = (sizeof(BlockLink_t) + (HEAP_BYTE_ALIGNMENT - 1)) & ~(
                                          (size_t) HEAP_BYTE_ALIGNMENT - 1);
/* ��С�ڴ���С��һ������������ɷ��Ѻ������ͷ�� */
#define HEAP_MINIMUM_BLOCK_SIZE    (heapStructSize * 2)

/* ��̬�ڴ�� */
static uint8_t rtos_memory_pool[RTOS_MEMORY_POOL_SIZE] __attribute__((aligned(HEAP_BYTE_ALIGNMENT)));
/* �����������ʼ�ͽ������ */
static BlockLink_t start, *blockLinkEnd = NULL;
/* ʣ������ڴ��С */
static size_t freeBytesRemaining = 0U;
/* ���ڱ���ڴ���Ƿ��ѱ�����ı�־λ */
static size_t blockAllocatedBit = 0;

/* ��ʼ�����ڴ� (�����״η���ʱ����) */
static void heapInit(void) {
    BlockLink_t *firstFreeBlock;
    uint8_t *alignedHeap;
    size_t address = (size_t) rtos_memory_pool;
    size_t totalHeapSize = RTOS_MEMORY_POOL_SIZE;

    if ((address & (HEAP_BYTE_ALIGNMENT - 1)) != 0) {
        address += (HEAP_BYTE_ALIGNMENT - (address & (HEAP_BYTE_ALIGNMENT - 1)));
        totalHeapSize -= address - (size_t) rtos_memory_pool;
    }
    alignedHeap = (uint8_t *) address;

    start.nextFreeBlock = (BlockLink_t *) alignedHeap;
    start.blockSize = (size_t) 0;

    address = ((size_t) alignedHeap) + totalHeapSize - heapStructSize;
    blockLinkEnd = (BlockLink_t *) address;
    blockLinkEnd->blockSize = 0;
    blockLinkEnd->nextFreeBlock = NULL;

    firstFreeBlock = (BlockLink_t *) alignedHeap;
    firstFreeBlock->blockSize = address - (size_t) firstFreeBlock;
    firstFreeBlock->nextFreeBlock = blockLinkEnd;

    freeBytesRemaining = firstFreeBlock->blockSize;
    blockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * 8) - 1);
}

/* ��һ���ڴ����뵽���������У������������ڿ��п�ĺϲ� */
static void insertBlockIntoFreeList(BlockLink_t *blockToInsert) {
    BlockLink_t *iterator;
    uint8_t *puc;

    /* ���������ҵ����Բ������ͷſ��λ�� (����ַ����) */
    for (iterator = &start; iterator->nextFreeBlock < blockToInsert;
         iterator = iterator->nextFreeBlock) {
        /* ʲô����������ֻ���ҵ�λ�� */
    }

    /* �����¿��Ƿ��������ָ��Ŀ������������� */
    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        /* ���ڣ��ϲ����� */
        iterator->blockSize += blockToInsert->blockSize;
        /* �ϲ����¿���ǵ�����ָ��Ŀ��� */
        blockToInsert = iterator;
    } else {
        /* �����ڣ����¿����ӵ����������� */
        blockToInsert->nextFreeBlock = iterator->nextFreeBlock;
    }

    /* �����¿��Ƿ������Ŀ����� */
    puc = (uint8_t *) blockToInsert;
    if ((puc + blockToInsert->blockSize) == (uint8_t *) iterator->nextFreeBlock) {
        if (iterator->nextFreeBlock != blockLinkEnd) {
            /* ���ڣ��ϲ����� */
            blockToInsert->blockSize += iterator->nextFreeBlock->blockSize;
            blockToInsert->nextFreeBlock = iterator->nextFreeBlock->nextFreeBlock;
        }
    }

    /* �����һ��û�кϲ�����ô����������һ���ڵ���Ҫָ���¿� */
    if (iterator != blockToInsert) {
        iterator->nextFreeBlock = blockToInsert;
    }
}

/* ��̬�ڴ���亯�� */
void *rtos_malloc(const size_t wantedSize) {
    BlockLink_t *block, *previousBlock, *newBlockLink;
    void *pvReturn = NULL;
    uint32_t primask_status;

    MY_RTOS_ENTER_CRITICAL(primask_status); {
        if (blockLinkEnd == NULL) {
            heapInit();
        }

        if ((wantedSize > 0) && ((wantedSize & blockAllocatedBit) == 0)) {
            size_t xTotalSize = heapStructSize + wantedSize;
            if ((xTotalSize & (HEAP_BYTE_ALIGNMENT - 1)) != 0) {
                xTotalSize += (HEAP_BYTE_ALIGNMENT - (xTotalSize & (HEAP_BYTE_ALIGNMENT - 1)));
            }

            if (xTotalSize <= freeBytesRemaining) {
                previousBlock = &start;
                block = start.nextFreeBlock;
                while ((block->blockSize < xTotalSize) && (block->nextFreeBlock != NULL)) {
                    previousBlock = block;
                    block = block->nextFreeBlock;
                }

                if (block != blockLinkEnd) {
                    pvReturn = (void *) (((uint8_t *) block) + heapStructSize);
                    previousBlock->nextFreeBlock = block->nextFreeBlock;

                    if ((block->blockSize - xTotalSize) > HEAP_MINIMUM_BLOCK_SIZE) {
                        newBlockLink = (BlockLink_t *) (((uint8_t *) block) + xTotalSize);
                        newBlockLink->blockSize = block->blockSize - xTotalSize;
                        block->blockSize = xTotalSize;
                        insertBlockIntoFreeList(newBlockLink);
                    }

                    freeBytesRemaining -= block->blockSize;
                    block->blockSize |= blockAllocatedBit;
                    block->nextFreeBlock = NULL;
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
    BlockLink_t *link;
    uint32_t primask_status;

    puc -= heapStructSize;
    link = (BlockLink_t *) puc;

    if (((link->blockSize & blockAllocatedBit) != 0) && (link->nextFreeBlock == NULL)) {
        link->blockSize &= ~blockAllocatedBit;
        MY_RTOS_ENTER_CRITICAL(primask_status); {
            freeBytesRemaining += link->blockSize;
            insertBlockIntoFreeList(link);
        }
        MY_RTOS_EXIT_CRITICAL(primask_status);
    }
}

//====================== ��̬�ڴ���� ======================


//================= Task ================


static Task_t *allTaskListHead = NULL;
static Task_t *currentTask = NULL;
static Task_t *idleTask = NULL;
static uint32_t nextTaskId = 0;

static Task_t *readyTaskLists[MY_RTOS_MAX_PRIORITIES];
// ��ʱ�����б�������ʱ�����򣨲����򣬽����б�
static Task_t *delayedTaskListHead = NULL;
// ���ڿ��ٲ���������ȼ����������bitmap
static volatile uint32_t topReadyPriority = 0;

#define SIZEOF_TASK_T sizeof(Task_t)


void MyRTOS_Init(void) {
    allTaskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    nextTaskId = 0;
    for (int i = 0; i < MY_RTOS_MAX_PRIORITIES; i++) {
        readyTaskLists[i] = NULL;
    }
    delayedTaskListHead = NULL;
    topReadyPriority = 0;
    DBG_PRINTF("MyRTOS Initialized. Task list cleared and memory manager reset.\n");
}

static Task_t *find_task_by_id(uint32_t task_id) {
    Task_t *p = allTaskListHead;
    while (p != NULL) {
        if (p->taskId == task_id) return p;
        p = p->pNextTask;
    }
    return NULL;
}

static void addTaskToReadyList(Task_t *task) {
    if (task == NULL || task->priority >= MY_RTOS_MAX_PRIORITIES) {
        return;
    }

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    // ��Ǹ����ȼ��о���������
    topReadyPriority |= (1UL << task->priority);

    // ��������뵽��Ӧ���ȼ������β��
    task->pNextReady = NULL;
    if (readyTaskLists[task->priority] == NULL) {
        // ���Ǹ����ȼ��ĵ�һ������
        readyTaskLists[task->priority] = task;
        task->pPrevReady = NULL;
    } else {
        // �ҵ�����β��������
        Task_t *pLast = readyTaskLists[task->priority];
        while (pLast->pNextReady != NULL) {
            pLast = pLast->pNextReady;
        }
        pLast->pNextReady = task;
        task->pPrevReady = pLast;
    }
    task->state = TASK_STATE_READY;

    MY_RTOS_EXIT_CRITICAL(primask_status);
}

static void removeTaskFromList(Task_t **ppListHead, Task_t *pTaskToRemove) {
    if (pTaskToRemove == NULL) return;

    // ����ǰһ���ڵ�� next ָ��
    if (pTaskToRemove->pPrevReady != NULL) {
        pTaskToRemove->pPrevReady->pNextReady = pTaskToRemove->pNextReady;
    } else {
        // �Ƴ�����ͷ�ڵ�
        *ppListHead = pTaskToRemove->pNextReady;
    }

    // ���º�һ���ڵ�� prev ָ��
    if (pTaskToRemove->pNextReady != NULL) {
        pTaskToRemove->pNextReady->pPrevReady = pTaskToRemove->pPrevReady;
    }

    // �����Ƴ��ڵ��ָ��
    pTaskToRemove->pNextReady = NULL;
    pTaskToRemove->pPrevReady = NULL;

    // ��������ǴӾ����б����Ƴ�������Ҫ����Ƿ���Ҫ������ȼ�λͼ
    if (pTaskToRemove->state == TASK_STATE_READY) {
        if (readyTaskLists[pTaskToRemove->priority] == NULL) {
            topReadyPriority &= ~(1UL << pTaskToRemove->priority);
        }
    }
}


void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // ����͹���ģʽ���ȴ��ж�
    }
}

Task_t *Task_Create(void (*func)(void *), void *param, uint8_t priority) {
    // ������ȼ��Ƿ���Ч
    if (priority >= MY_RTOS_MAX_PRIORITIES) {
        DBG_PRINTF("Error: Invalid task priority %u.\n", priority);
        return NULL;
    }

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
        rtos_free(t);
        return NULL;
    }

    // ��ʼ��TCB��Ա
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    // t->state �� prvAddTaskToReadyList ������
    t->taskId = nextTaskId++;
    t->stack_base = stack;
    t->pNextTask = NULL;
    t->pNextReady = NULL;
    t->pPrevReady = NULL;
    t->priority = priority;

    uint32_t *sp = stack + STACK_SIZE;
    sp = (uint32_t *) (((uintptr_t) sp) & ~0x7u);
    sp -= 8;
    sp[0] = (uint32_t) param;
    sp[1] = 0x01010101;
    sp[2] = 0x02020202;
    sp[3] = 0x03030303;
    sp[4] = 0x12121212;
    sp[5] = 0x00000000;
    sp[6] = ((uint32_t) func) | 1u;
    sp[7] = 0x01000000;
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

    // ��ӵ�������������
    if (allTaskListHead == NULL) {
        allTaskListHead = t;
    } else {
        Task_t *p = allTaskListHead;
        while (p->pNextTask != NULL) {
            p = p->pNextTask;
        }
        p->pNextTask = t;
    }

    addTaskToReadyList(t);

    MY_RTOS_EXIT_CRITICAL(primask_status);

    DBG_PRINTF("Task %lu created with priority %u. Stack top: %p, Initial SP: %p\n", t->taskId, t->priority,
               &stack[STACK_SIZE - 1], t->sp);
    return t;
}

// ɾ������
int Task_Delete(const Task_t *task_h) {
    // ������ɾ�� NULL ������������
    if (task_h == idleTask) {
        return -1;
    }
    //ɾ����ǰ����
    if (task_h == NULL) {
        task_h = currentTask;
    }
    // ��Ҫ�޸����� TCB �����ݣ�������Ҫһ���� const ��ָ��
    Task_t *task_to_delete = (Task_t *) task_h;
    uint32_t primask_status;
    int trigger_yield = 0; // �Ƿ���Ҫ�ں���ĩβ�������ȵı�־

    MY_RTOS_ENTER_CRITICAL(primask_status);

    //��������ǰ״̬���Ӷ�Ӧ��״̬�������Ƴ�
    switch (task_to_delete->state) {
        case TASK_STATE_READY:
            removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
            break;
        case TASK_STATE_DELAYED:
            removeTaskFromList(&delayedTaskListHead, task_to_delete);
            break;
        case TASK_STATE_BLOCKED:
            // ����״̬���������κλ�б��У��������
            break;
        default:
            // ����״̬����Ч״̬
            break;
    }

    // ������״̬���Ϊδʹ�ã���ֹ�����ط������
    task_to_delete->state = TASK_STATE_UNUSED;

    //�Զ��ͷ�������е����л���������ֹ����
    Mutex_t *p_mutex = task_to_delete->held_mutexes_head;
    while (p_mutex != NULL) {
        Mutex_t *next_mutex = p_mutex->next_held_mutex;

        // �ֶ�����
        p_mutex->locked = 0;
        p_mutex->owner = (uint32_t) -1;
        p_mutex->owner_tcb = NULL;
        p_mutex->next_held_mutex = NULL; // �ӳ������жϿ�

        // ��������������ڵȴ����������������
        if (p_mutex->waiting_mask != 0) {
            Task_t *p_task = allTaskListHead; // �����������������ҵȴ���
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (p_mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        //��������
                        p_mutex->waiting_mask &= ~(1UL << p_task->taskId); // ����ȴ���־
                        addTaskToReadyList(p_task); // ������Żؾ����б�
                        // ����Ƿ���Ҫ��ռ
                        if (p_task->priority > currentTask->priority) {
                            trigger_yield = 1;
                        }
                    }
                }
                p_task = p_task->pNextTask;
            }
        }
        p_mutex = next_mutex;
    }

    Task_t *prev = NULL;
    Task_t *curr = allTaskListHead;
    while (curr != NULL && curr != task_to_delete) {
        prev = curr;
        curr = curr->pNextTask; // <<< �޸ĵ�: ʹ�� pNextTask ָ��
    }

    if (curr == NULL) {
        // �����ڹ��������У��������𻵣�ֱ�ӷ���
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return -2;
    }

    if (prev == NULL) {
        allTaskListHead = curr->pNextTask;
    } else {
        prev->pNextTask = curr->pNextTask;
    }

    //���ɾ�����ǵ�ǰ�������е����񣬱���������������
    if (curr == currentTask) {
        trigger_yield = 1;
    }

    rtos_free(curr->stack_base);
    rtos_free(curr);

    MY_RTOS_EXIT_CRITICAL(primask_status);

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

    //�Ӿ����б����Ƴ���ǰ����
    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);

    currentTask->delay = tick;
    currentTask->state = TASK_STATE_DELAYED;

    //��: ͷ�巨
    currentTask->pNextReady = delayedTaskListHead;
    currentTask->pPrevReady = NULL;
    if (delayedTaskListHead != NULL) {
        delayedTaskListHead->pPrevReady = currentTask;
    }
    delayedTaskListHead = currentTask;

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

// �������������
void *schedule_next_task(void) {
    Task_t *nextTaskToRun = NULL;

    if (topReadyPriority == 0) {
        // û�о�������ֻ�����п�������
        nextTaskToRun = idleTask;
    } else {
        // AI����ħ������: ʹ�� CLZ (Count Leading Zeros) ָ������ҵ���ߵ���λ
        // 31 - __CLZ(topReadyPriority) ��ֱ�ӵõ����λ������
        uint32_t highestPriority = 31 - __CLZ(topReadyPriority);

        // �Ӹ����ȼ��ľ�������ͷȡ������
        nextTaskToRun = readyTaskLists[highestPriority];

        // ʵ��ͬ���ȼ��������ѯ (Round-Robin)
        // ��������ȼ��ж�������򽫵�ǰѡ�е������Ƶ�����β��
        if (nextTaskToRun != NULL && nextTaskToRun->pNextReady != NULL) {
            readyTaskLists[highestPriority] = nextTaskToRun->pNextReady;
            nextTaskToRun->pNextReady->pPrevReady = NULL;

            Task_t *pLast = readyTaskLists[highestPriority];
            while (pLast->pNextReady != NULL) {
                pLast = pLast->pNextReady;
            }
            pLast->pNextReady = nextTaskToRun;
            nextTaskToRun->pPrevReady = pLast;
            nextTaskToRun->pNextReady = NULL;
        }
    }

    currentTask = nextTaskToRun;

    if (currentTask == NULL) {
        // ��Ӧ�ù�
        return NULL;
    }

    return currentTask->sp;
}

void Task_StartScheduler(void) {
    // ������������ ���ȼ���� (0)
    idleTask = Task_Create(MyRTOS_Idle_Task, NULL, 0);
    if (idleTask == NULL) {
        DBG_PRINTF("Error: Failed to create Idle Task!\n");
        while (1);
    }

    SCB->VTOR = (uint32_t) 0x08000000;
    DBG_PRINTF("Starting scheduler...\n");

    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0x00);
    if (SysTick_Config(SystemCoreClock / 1000)) {
        DBG_PRINTF("Error: SysTick_Config failed\n");
        while (1);
    }

    // ��һ�ε���,�ֶ�ѡ����һ������
    schedule_next_task();

    __asm volatile(
        "ldr r0, =0xE000ED08\n"
        "ldr r0, [r0]\n"
        "ldr r0, [r0]\n"
        "msr msp, r0\n"
    );
    __asm volatile("svc 0");
    for (;;);
}


//================= Task ================

//=================== �ź���==================
int Task_Notify(uint32_t task_id) {
    // ��������������ٽ���֮����У���߲�����
    Task_t *task_to_notify = find_task_by_id(task_id);
    if (task_to_notify == NULL) {
        return -1; // ��ЧID
    }

    uint32_t primask_status;
    int trigger_yield = 0; // �Ƿ���Ҫ��ռ���ȵı�־

    MY_RTOS_ENTER_CRITICAL(primask_status);

    // ��������Ƿ�ȷʵ�ڵȴ�֪ͨ
    if (task_to_notify->is_waiting_notification && task_to_notify->state == TASK_STATE_BLOCKED) {
        // ����ȴ���־
        task_to_notify->is_waiting_notification = 0;

        //������������ӵ������б��У�ʹ����Ա�����
        addTaskToReadyList(task_to_notify);

        //����Ƿ���Ҫ��ռ����������ѵ��������ȼ����ڵ�ǰ����
        if (task_to_notify->priority > currentTask->priority) {
            trigger_yield = 1;
        }
    }

    MY_RTOS_EXIT_CRITICAL(primask_status);

    if (trigger_yield) {
        MY_RTOS_YIELD();
    }

    return 0;
}

void Task_Wait(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);

    currentTask->is_waiting_notification = 1;
    currentTask->state = TASK_STATE_BLOCKED;

    MY_RTOS_YIELD();

    MY_RTOS_EXIT_CRITICAL(primask_status);

    // __ISB ȷ����ˮ�߱�ˢ��
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
        MY_RTOS_ENTER_CRITICAL(primask_status);
        if (!mutex->locked) {
            mutex->locked = 1;
            mutex->owner = currentTask->taskId;
            mutex->owner_tcb = currentTask;
            mutex->next_held_mutex = currentTask->held_mutexes_head;
            currentTask->held_mutexes_head = mutex;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return;
        }
        // ����ռ��,����ȴ�
        if (currentTask->taskId < 32) {
            mutex->waiting_mask |= (1 << currentTask->taskId);
        }
        // �Ӿ����б��Ƴ�
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;

        // ��������
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);
        // �����񱻻��Ѻ󣬻���������ִ��
    }
}

void Mutex_Unlock(Mutex_t *mutex) {
    uint32_t primask_status;
    int trigger_yield = 0;

    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner == currentTask->taskId) {
        if (currentTask->held_mutexes_head == mutex) {
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
        mutex->next_held_mutex = NULL;

        // �ͷ���
        mutex->locked = 0;
        mutex->owner = (uint32_t) -1;
        mutex->owner_tcb = NULL;

        // ���ѵȴ�������
        if (mutex->waiting_mask != 0) {
            Task_t *p_task = allTaskListHead; // <<< ������������
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        // ����ȴ���־
                        mutex->waiting_mask &= ~(1 << p_task->taskId);
                        // ��ӻؾ����б�
                        addTaskToReadyList(p_task);
                        // �����������������ȼ����ڵ�ǰ��������Ҫ����
                        if (p_task->priority > currentTask->priority) {
                            trigger_yield = 1;
                        }
                    }
                }
                p_task = p_task->pNextTask;
            }
        }

        if (trigger_yield) {
            MY_RTOS_YIELD();
        }
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

//============== ������ =============


//====================== ��Ϣ���� ======================
static void addTaskToPrioritySortedList(Task_t **listHead, Task_t *taskToInsert) {
    if (*listHead == NULL || (*listHead)->priority <= taskToInsert->priority) {
        // ���뵽ͷ�� (�б�Ϊ�ջ����������ȼ����߻����)
        taskToInsert->pNextEvent = *listHead;
        *listHead = taskToInsert;
    } else {
        // �������Ҳ����
        Task_t *iterator = *listHead;
        while (iterator->pNextEvent != NULL && iterator->pNextEvent->priority > taskToInsert->priority) {
            iterator = iterator->pNextEvent;
        }
        taskToInsert->pNextEvent = iterator->pNextEvent;
        iterator->pNextEvent = taskToInsert;
    }
}

QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize) {
    if (length == 0 || itemSize == 0) {
        return NULL;
    }

    Queue_t *queue = rtos_malloc(sizeof(Queue_t));
    if (queue == NULL) {
        return NULL;
    }

    queue->storage = (uint8_t *)rtos_malloc(length * itemSize);
    if (queue->storage == NULL) {
        rtos_free(queue);
        return NULL;
    }
    queue->length = length;
    queue->itemSize = itemSize;
    queue->waitingCount = 0;
    queue->writePtr = queue->storage;
    queue->readPtr = queue->storage;
    queue->sendWaitList = NULL;
    queue->receiveWaitList = NULL;
    return queue;
}

void Queue_Delete(QueueHandle_t delQueue) {
    Queue_t* queue = delQueue;
    if (queue == NULL) return;
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    // �������еȴ������� (���ǽ��� Send/Receive ������ʧ�ܷ���)
    while(queue->sendWaitList) {
        Task_t* taskToWake = queue->sendWaitList;
        queue->sendWaitList = taskToWake->pNextEvent;
        addTaskToReadyList(taskToWake);
    }
    while(queue->receiveWaitList) {
        Task_t* taskToWake = queue->receiveWaitList;
        queue->receiveWaitList = taskToWake->pNextEvent;
        addTaskToReadyList(taskToWake);
    }
    rtos_free(queue->storage);
    rtos_free(queue);
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

int Queue_Send(QueueHandle_t queue, const void *item, int block) {
    Queue_t* pQueue = queue;
    if (pQueue == NULL) return 0;
    uint32_t primask_status;
    while(1) { // ѭ����������������
        MY_RTOS_ENTER_CRITICAL(primask_status);
        // ���ȼ����أ�����Ƿ��и����ȼ������ڵȴ�����
        if (pQueue->receiveWaitList != NULL) {
            // ֱ�ӽ����ݴ��ݸ��ȴ���������ȼ����񣬲��������д洢��
            Task_t *taskToWake = pQueue->receiveWaitList;
            pQueue->receiveWaitList = taskToWake->pNextEvent; // �ӵȴ��б��Ƴ�
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            // ���Ѹ�����
            taskToWake->eventObject = NULL;
            taskToWake->eventData = NULL;
            addTaskToReadyList(taskToWake);
            // ��������ѵ��������ȼ����ߣ���������
            if (taskToWake->priority > currentTask->priority) {
                MY_RTOS_YIELD();
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // ���ͳɹ�
        }
        // û�������ڵȴ������Է�����л�����
        if (pQueue->waitingCount < pQueue->length) {
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // ���ͳɹ�
        }
        // ��������
        if (block == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0; // ��������ֱ�ӷ���ʧ��
        }
        // ������ǰ����
        currentTask->eventObject = pQueue;
        addTaskToPrioritySortedList(&pQueue->sendWaitList, currentTask);
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);
        // �����Ѻ󣬽��ص� while(1) ѭ���Ŀ�ʼ�����³��Է���
    }
}

int Queue_Receive(QueueHandle_t queue, void *buffer, int block) {
    Queue_t* pQueue = queue;
    if (pQueue == NULL) return 0;
    uint32_t primask_status;
    while(1) { // ѭ����������������
        MY_RTOS_ENTER_CRITICAL(primask_status);
        if (pQueue->waitingCount > 0) {
            // �Ӷ��л�������ȡ����
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;
            // ����Ƿ��������ڵȴ����� (����������ȼ���)
            if (pQueue->sendWaitList != NULL) {
                Task_t *taskToWake = pQueue->sendWaitList;
                pQueue->sendWaitList = taskToWake->pNextEvent;

                // �������������Լ�ȥ����
                taskToWake->eventObject = NULL;
                addTaskToReadyList(taskToWake);

                // ��������ѵ��������ȼ����ߣ���������
                if (taskToWake->priority > currentTask->priority) {
                    MY_RTOS_YIELD();
                }
            }

            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // ���ճɹ�
        }

        // ����Ϊ��
        if (block == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0; // ��������ֱ�ӷ���ʧ��
        }

        // ������ǰ����
        currentTask->eventObject = pQueue;
        currentTask->eventData = buffer; // ������ջ������ĵ�ַ��
        addTaskToPrioritySortedList(&pQueue->receiveWaitList, currentTask);
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;

        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);

        // �����Ѻ������ֿ��ܣ�
        // 1. ��Queue_Sendֱ�Ӵ��������� -> pEventObject����NULL���������
        // 2. ��Queue_Delete���� -> pEventObject���ܲ���NULL��ѭ����ʧ��
        if(currentTask->eventObject == NULL) {
             // �ɹ��� Queue_Send ���Ѳ����յ�����
             return 1;
        }
        // ���򣬻ص�ѭ����������
    }
}



//=========== Handler ============

void SysTick_Handler(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    Task_t *p = delayedTaskListHead;
    Task_t *pNext = NULL;

    // ������ʱ����
    while (p != NULL) {
        pNext = p->pNextReady; // �ȱ�����һ���ڵ㣬��Ϊ��ǰ�ڵ���ܱ��Ƴ�

        if (p->delay > 0) {
            p->delay--;
        }

        if (p->delay == 0) {
            // ��ʱ�������������ƻؾ����б�
            removeTaskFromList(&delayedTaskListHead, p);
            addTaskToReadyList(p);
        }

        p = pNext;
    }

    // ����Ƿ��и������ȼ��������Ѿ�����������ǣ�����Ҫ����
    //�� ÿ��SysTick���������
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
