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


//====================== Internal Data Structures & Defines ======================

// ���䶨��
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

    void (*func)(void *); // ������
    void *param; // �������
    uint64_t delay; // ��ʱ
    volatile uint32_t notification;
    volatile uint8_t is_waiting_notification;
    volatile TaskState_t state; // ����״̬
    uint32_t taskId; // ����ID
    uint32_t *stack_base; // ջ����ַ,����free
    uint8_t priority; //�������ȼ�
    struct Task_t *pNextTask; //����������������
    struct Task_t *pNextReady; //���ھ�������ʱ����
    struct Task_t *pPrevReady; //����˫������,����ɾ�� O(1)���Ӷ�
    Mutex_t *held_mutexes_head;
    void *eventObject; // ָ�����ڵȴ����ں˶���
    void *eventData; // ���ڴ������¼���ص�����ָ�� (����Ϣ��Դ/Ŀ�ĵ�ַ)
    struct Task_t *pNextEvent; // ���ڹ����ں˶���ĵȴ���������
} Task_t;

typedef struct Timer_t {
    TimerCallback_t callback; // ��ʱ������ʱִ�еĻص�����
    void *arg; // ���ݸ��ص������Ķ������
    uint32_t initialDelay; // �״δ�����ʱ (in ticks)
    uint32_t period; // ���� (in ticks), ���Ϊ0���ǵ��ζ�ʱ��
    volatile uint32_t expiryTime; // ��һ�ε���ʱ�ľ���ϵͳtick
    struct Timer_t *pNext; // ���ڹ������ʱ������
    volatile uint8_t active; // ��ʱ���Ƿ��ڻ״̬ (0:inactive, 1:active)
} Timer_t;

typedef struct Queue_t {
    uint8_t *storage; // ָ����д洢����ָ��
    uint32_t length; // ������������ɵ���Ϣ��
    uint32_t itemSize; // ÿ����Ϣ�Ĵ�С
    volatile uint32_t waitingCount; // ��ǰ�����е���Ϣ��
    uint8_t *writePtr; // ��һ��Ҫд�����ݵ�λ��
    uint8_t *readPtr; // ��һ��Ҫ��ȡ���ݵ�λ��
    // �ȴ��б� (�����������ȼ�����)
    Task_t *sendWaitList;
    Task_t *receiveWaitList;
} Queue_t;


//====================== Static Global Variables ======================

/*----- Memory Management -----*/
typedef struct BlockLink_t {
    struct BlockLink_t *nextFreeBlock;
    size_t blockSize;
} BlockLink_t;

static const size_t heapStructSize = (sizeof(BlockLink_t) + (HEAP_BYTE_ALIGNMENT - 1)) & ~(
                                         (size_t) HEAP_BYTE_ALIGNMENT - 1);
#define HEAP_MINIMUM_BLOCK_SIZE    (heapStructSize * 2)
static uint8_t rtos_memory_pool[RTOS_MEMORY_POOL_SIZE] __attribute__((aligned(HEAP_BYTE_ALIGNMENT)));
static BlockLink_t start, *blockLinkEnd = NULL;
static size_t freeBytesRemaining = 0U;
static size_t blockAllocatedBit = 0;

/*----- System & Task -----*/
static volatile uint64_t systemTickCount = 0;
static TaskHandle_t allTaskListHead = NULL;
static TaskHandle_t currentTask = NULL;
static TaskHandle_t idleTask = NULL;
static uint32_t nextTaskId = 0;
static TaskHandle_t readyTaskLists[MY_RTOS_MAX_PRIORITIES];
static TaskHandle_t delayedTaskListHead = NULL;
static volatile uint32_t topReadyPriority = 0;

/*----- Timer -----*/
typedef enum { TIMER_CMD_START, TIMER_CMD_STOP, TIMER_CMD_DELETE } TimerCommandType_t;

typedef struct {
    TimerCommandType_t command;
    TimerHandle_t timer;
} TimerCommand_t;

static TaskHandle_t timerServiceTaskHandle = NULL;
static QueueHandle_t timerCommandQueue = NULL;
static TimerHandle_t activeTimerListHead = NULL;


//====================== Function Prototypes ======================

/*----- Memory Management -----*/
static void heapInit(void);

static void insertBlockIntoFreeList(BlockLink_t *blockToInsert);

static void *rtos_malloc(size_t wantedSize);

static void rtos_free(void *pv);

/*----- System Core & Scheduler -----*/
void *schedule_next_task(void);

static void MyRTOS_Idle_Task(void *pv);

/*----- Task Management -----*/
static void addTaskToSortedDelayList(TaskHandle_t task);

static void addTaskToReadyList(TaskHandle_t task);

static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t pTaskToRemove);

static void removeTaskFromEventList(Task_t **ppEventList, Task_t *pTaskToRemove);

static void addTaskToPrioritySortedList(Task_t **listHead, Task_t *taskToInsert);

/*----- Timer Management -----*/
static void TimerServiceTask(void *pv);

static void insertTimerIntoActiveList(Timer_t *timerToInsert);

static void removeTimerFromActiveList(Timer_t *timerToRemove);

static void processExpiredTimers(void);

static int sendCommandToTimerTask(TimerHandle_t timer, TimerCommandType_t cmd, int block);


//====================== Function Implementations ======================

//====================== ��̬�ڴ���� ======================

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
static void *rtos_malloc(const size_t wantedSize) {
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
static void rtos_free(void *pv) {
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


//==========================System Core====================================
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

uint64_t MyRTOS_GetTick(void) {
    uint64_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    const uint64_t tick_value = systemTickCount;
    MY_RTOS_EXIT_CRITICAL(primask_status);
    return tick_value;
}

void Task_StartScheduler(void) {
    // ������������ ���ȼ���� (0)
    idleTask = Task_Create(MyRTOS_Idle_Task, 64,NULL, 0);
    if (idleTask == NULL) {
        DBG_PRINTF("Error: Failed to create Idle Task!\n");
        while (1);
    }

    //����һ�����У�������ʱ���������������� ����10���
    timerCommandQueue = Queue_Create(10, sizeof(TimerCommand_t));
    if (timerCommandQueue == NULL) {
        DBG_PRINTF("Error: Failed to create Timer Command Queue!\n");
        while (1);
    }
    // MY_RTOS_MAX_PRIORITIES - 1 ��������ȼ�
    timerServiceTaskHandle = Task_Create(TimerServiceTask, 256, NULL, MY_RTOS_MAX_PRIORITIES - 1);
    if (timerServiceTaskHandle == NULL) {
        DBG_PRINTF("Error: Failed to create Timer Service Task!\n");
        while (1);
    }
    SCB->VTOR = (uint32_t) 0x08000000;
    DBG_PRINTF("Starting scheduler...\n");

    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0x00);
    if (SysTick_Config(SystemCoreClock / MY_RTOS_TICK_RATE_HZ)) {
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

static void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // ����͹���ģʽ���ȴ��ж�
    }
}


//================= Task Management ================

// �ҵ����滻ԭ���� addTaskToSortedDelayList ����
static void addTaskToSortedDelayList(TaskHandle_t task) {
    // delay �ǻ��ѵľ���ʱ��
    const uint64_t wakeUpTime = task->delay;

    if (delayedTaskListHead == NULL || wakeUpTime < delayedTaskListHead->delay) {
        // ���뵽ͷ�� (����Ϊ�ջ��������ͷ������绽��)
        task->pNextReady = delayedTaskListHead;
        task->pPrevReady = NULL;
        if (delayedTaskListHead != NULL) {
            delayedTaskListHead->pPrevReady = task;
        }
        delayedTaskListHead = task;
    } else {
        // �������Ҳ���㣬 �ҵ�һ�� iterator��ʹ��������Ӧ�ñ�������������
        Task_t *iterator = delayedTaskListHead;

        // ʹ�� <= ȷ������ͬ����ʱ��������� FIFO (�Ƚ��ȳ�) ��
        while (iterator->pNextReady != NULL && iterator->pNextReady->delay <= wakeUpTime) {
            iterator = iterator->pNextReady;
        }

        // �˿�, ������Ӧ�ñ����뵽 iterator �� iterator->pNextReady ֮��

        // ��������� next ָ�� iterator ����һ���ڵ�
        task->pNextReady = iterator->pNextReady;
        if (iterator->pNextReady != NULL) {
            iterator->pNextReady->pPrevReady = task;
        }
        iterator->pNextReady = task;
        task->pPrevReady = iterator;
    }
}

static void addTaskToReadyList(TaskHandle_t task) {
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

static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t pTaskToRemove) {
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

static void removeTaskFromEventList(Task_t **ppEventList, Task_t *pTaskToRemove) {
    if (*ppEventList == pTaskToRemove) {
        *ppEventList = pTaskToRemove->pNextEvent;
    } else {
        Task_t *iterator = *ppEventList;
        while (iterator != NULL && iterator->pNextEvent != pTaskToRemove) {
            iterator = iterator->pNextEvent;
        }
        if (iterator != NULL) {
            iterator->pNextEvent = pTaskToRemove->pNextEvent;
        }
    }
    pTaskToRemove->pNextEvent = NULL; // ����ָ��
}

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

TaskHandle_t Task_Create(void (*func)(void *), uint16_t stack_size, void *param, uint8_t priority) {
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
    uint32_t stack_size_bytes = stack_size * sizeof(uint32_t);
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
    t->taskId = nextTaskId++;
    t->stack_base = stack;
    t->pNextTask = NULL;
    t->pNextReady = NULL;
    t->pPrevReady = NULL;
    t->priority = priority;
    t->held_mutexes_head = NULL;
    t->eventObject = NULL;
    t->eventData = NULL;
    t->pNextEvent = NULL;

    uint32_t *sp = stack + stack_size; // stack_size is in words (uint32_t)
    sp = (uint32_t *) (((uintptr_t) sp) & ~0x7u);

    /* Ӳ���Զ������ջ֡ (R0-R3, R12, LR, PC, xPSR) */
    sp--;
    *sp = 0x01000000; // xPSR (Thumb bit must be 1)
    sp--;
    *sp = ((uint32_t) func) | 1u; // PC (��ڵ�)
    sp--;
    *sp = 0; // LR (���񷵻ص�ַ����Ϊ0��������ɱ����)
    sp--;
    *sp = 0x12121212; // R12
    sp--;
    *sp = 0x03030303; // R3
    sp--;
    *sp = 0x02020202; // R2
    sp--;
    *sp = 0x01010101; // R1
    sp--;
    *sp = (uint32_t) param; // R0 (����)

    /* ����ֶ������ͨ�üĴ��� (R4 - R11) �� EXC_RETURN */
    sp--;
    *sp = 0x0B0B0B0B; // R11
    sp--;
    *sp = 0x0A0A0A0A; // R10
    sp--;
    *sp = 0x09090909; // R9
    sp--;
    *sp = 0x08080808; // R8
    sp--;
    *sp = 0x07070707; // R7
    sp--;
    *sp = 0x06060606; // R6
    sp--;
    *sp = 0x05050505; // R5
    sp--;
    *sp = 0x04040404; // R4
    sp--;
    *sp = 0xFFFFFFFD; // EXC_RETURN: ָʾ����ʱ�ָ�FPU������

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
               &stack[stack_size - 1], t->sp);
    return t;
}

int Task_Delete(TaskHandle_t task_h) {
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
    if (task_to_delete->state == TASK_STATE_READY) {
        removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
    } else {
        removeTaskFromList(&delayedTaskListHead, task_to_delete);
    }
    //����������ڵȴ��ں˶�������У�����Ӷ���ĵȴ��б����Ƴ���
    if (task_to_delete->state == TASK_STATE_BLOCKED && task_to_delete->eventObject != NULL) {
        // ��������ֻ�����˶��У�������ź������¼���ȣ�Ҳ��Ҫ�����ﴦ��
        Queue_t *pQueue = (Queue_t *) task_to_delete->eventObject;
        // �������ڷ��͵ȴ��б����յȴ��б�
        removeTaskFromEventList(&pQueue->sendWaitList, task_to_delete);
        removeTaskFromEventList(&pQueue->receiveWaitList, task_to_delete);
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

// �ҵ����滻ԭ���� Task_Delay ����
void Task_Delay(uint32_t tick) {
    if (tick == 0) return;
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    //�Ӿ����б����Ƴ���ǰ����
    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
    currentTask->delay = MyRTOS_GetTick() + tick;
    currentTask->state = TASK_STATE_DELAYED;
    //��������뵽�������ʱ����
    addTaskToSortedDelayList(currentTask);
    MY_RTOS_EXIT_CRITICAL(primask_status);

    MY_RTOS_YIELD();
    __ISB();
}

int Task_Notify(TaskHandle_t task_h) {
    // ��������������ٽ���֮����У���߲�����

    uint32_t primask_status;
    int trigger_yield = 0; // �Ƿ���Ҫ��ռ���ȵı�־

    MY_RTOS_ENTER_CRITICAL(primask_status);

    // ��������Ƿ�ȷʵ�ڵȴ�֪ͨ
    if (task_h->is_waiting_notification && task_h->state == TASK_STATE_BLOCKED) {
        // ����ȴ���־
        task_h->is_waiting_notification = 0;

        //������������ӵ������б��У�ʹ����Ա�����
        addTaskToReadyList(task_h);

        //����Ƿ���Ҫ��ռ����������ѵ��������ȼ����ڵ�ǰ����
        if (task_h->priority > currentTask->priority) {
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

TaskState_t Task_GetState(TaskHandle_t task_h) {
    if (task_h == NULL) {
        return TASK_STATE_UNUSED;
    }
    return task_h->state;
}

uint8_t Task_GetPriority(TaskHandle_t task_h) {
    if (task_h == NULL) {
        return 0; // Or some invalid priority value
    }
    return task_h->priority;
}

TaskHandle_t Task_GetCurrentTaskHandle(void) {
    return currentTask;
}


//====================== ��Ϣ���� ======================

QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize) {
    if (length == 0 || itemSize == 0) {
        return NULL;
    }

    Queue_t *queue = rtos_malloc(sizeof(Queue_t));
    if (queue == NULL) {
        return NULL;
    }

    queue->storage = (uint8_t *) rtos_malloc(length * itemSize);
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
    Queue_t *queue = delQueue;
    if (queue == NULL) return;
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    // �������еȴ������� (���ǽ��� Send/Receive ������ʧ�ܷ���)
    while (queue->sendWaitList) {
        Task_t *taskToWake = queue->sendWaitList;
        queue->sendWaitList = taskToWake->pNextEvent;
        addTaskToReadyList(taskToWake);
    }
    while (queue->receiveWaitList) {
        Task_t *taskToWake = queue->receiveWaitList;
        queue->receiveWaitList = taskToWake->pNextEvent;
        addTaskToReadyList(taskToWake);
    }
    rtos_free(queue->storage);
    rtos_free(queue);
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) {
        return 0; // �����о���Ƿ���Ч
    }

    uint32_t primask_status;

    while (1) {
        // ʹ�� for ѭ���ṹ���Ա��ڱ����Ѻ���������������״̬
        MY_RTOS_ENTER_CRITICAL(primask_status);

        //���ȼ����أ�����Ƿ��и������ȼ����������ڵȴ���������
        if (pQueue->receiveWaitList != NULL) {
            // ֱ�ӽ����ݴ��ݸ��ȴ���������ȼ����񣬲��������д洢��
            Task_t *taskToWake = pQueue->receiveWaitList;

            // ��������ӽ��յȴ��б����Ƴ�
            removeTaskFromEventList(&pQueue->receiveWaitList, taskToWake);

            // �����������ѵ������Ƿ������˳�
            if (taskToWake->delay > 0) {
                // �����˵���������Ǵ���ʱ�����ģ����뽫������ʱ�б���Ҳ�Ƴ�
                removeTaskFromList(&delayedTaskListHead, taskToWake);
                taskToWake->delay = 0; // ���㣬��ֹ����
            }
            // ������ֱ�ӿ��������������ṩ�Ļ�����
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            // ����������¼���־���������������жϳɹ����Ĺؼ�
            taskToWake->eventObject = NULL;
            taskToWake->eventData = NULL;
            // ������Żؾ����б�
            addTaskToReadyList(taskToWake);
            // ��������ѵ��������ȼ����ߣ��������������л�
            if (taskToWake->priority > currentTask->priority) {
                MY_RTOS_YIELD();
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // ���ͳɹ�
        }

        //���Խ����ݷ�����л�����
        if (pQueue->waitingCount < pQueue->length) {
            // ����δ����������������
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            // ����дָ�����
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // ���ͳɹ�
        }

        //�������� ���� block_ticks �����Ƿ�����
        if (block_ticks == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0;
        }
        // ׼��������ǰ��������

        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        currentTask->eventObject = pQueue;
        // ����ǰ������뵽���͵ȴ��б������ȼ�����
        addTaskToPrioritySortedList(&pQueue->sendWaitList, currentTask);
        currentTask->delay = 0;
        // �������������������������Ҳ���뵽��ʱ�б�
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);

        //���񱻻��Ѻ󣬽����������ִ��
        // ��黽��ԭ��
        if (currentTask->eventObject == NULL) {
            // �� Queue_Receive �������ѣ���ζ�Ŷ��������пռ��ˡ�
            continue;
        }
        // �� SysTick ��ʱ���ѣ�eventObject ��Ȼָ����С�
        // ��Ҫ���Լ��ӷ��͵ȴ��б����������
        MY_RTOS_ENTER_CRITICAL(primask_status);
        removeTaskFromEventList(&pQueue->sendWaitList, currentTask);
        currentTask->eventObject = NULL; // �����¼�����
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return 0;
    }
}

int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) return 0;

    uint32_t primask_status;
    while (1) {
        MY_RTOS_ENTER_CRITICAL(primask_status);
        if (pQueue->waitingCount > 0) {
            //�������Ƿ�������
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;

            if (pQueue->sendWaitList != NULL) {
                Task_t *taskToWake = pQueue->sendWaitList;
                removeTaskFromEventList(&pQueue->sendWaitList, taskToWake);
                // ������Ƿ�Ҳ����ʱ�б���
                if (taskToWake->delay > 0) {
                    removeTaskFromList(&delayedTaskListHead, taskToWake);
                    taskToWake->delay = 0;
                }
                // �������������Լ�ȥ���Է���
                taskToWake->eventObject = NULL;
                addTaskToReadyList(taskToWake);

                if (taskToWake->priority > currentTask->priority) {
                    MY_RTOS_YIELD();
                }
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // ���ճɹ�
        }
        if (block_ticks == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0;
        }
        //׼������
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        //������еĵȴ��б�
        currentTask->eventObject = pQueue;
        currentTask->eventData = buffer;
        addTaskToPrioritySortedList(&pQueue->receiveWaitList, currentTask);
        currentTask->delay = 0;
        //���� block_ticks �����Ƿ������ʱ�б�
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        //��������
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);

        // ����Ǳ� Queue_Send �������ѣ����� eventObject �ᱻ��Ϊ NULL
        if (currentTask->eventObject == NULL) {
            return 1; // �ɹ�����
        }
        //SysTick ��ʱ����
        MY_RTOS_ENTER_CRITICAL(primask_status);
        removeTaskFromEventList(&pQueue->receiveWaitList, currentTask);
        currentTask->eventObject = NULL; // ����
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return 0; // ���س�ʱ
    }
}


//=======================Soft Timer==============================

TimerHandle_t Timer_Create(uint32_t delay, uint32_t period, TimerCallback_t callback, void *arg) {
    TimerHandle_t timer = rtos_malloc(sizeof(Timer_t));
    if (timer) {
        timer->callback = callback;
        timer->arg = arg;
        timer->initialDelay = delay;
        timer->period = period;
        timer->active = 0; // ��ʼΪ�ǻ״̬
        timer->pNext = NULL;
    }
    return timer;
}

int Timer_Start(const TimerHandle_t timer) {
    return sendCommandToTimerTask(timer, TIMER_CMD_START, 0); // 0��ʾ������
}

int Timer_Stop(const TimerHandle_t timer) {
    return sendCommandToTimerTask(timer, TIMER_CMD_STOP, 0);
}

int Timer_Delete(const TimerHandle_t timer) {
    return sendCommandToTimerTask(timer, TIMER_CMD_DELETE, 0);
}

static int sendCommandToTimerTask(const TimerHandle_t timer, const TimerCommandType_t cmd, const int block) {
    if (timerCommandQueue == NULL || timer == NULL) return -1;
    TimerCommand_t command = {.command = cmd, .timer = timer};
    // ������������У�ͨ��������
    if (Queue_Send(timerCommandQueue, &command, block)) {
        return 0; // ���ͳɹ�
    }
    return -1; // ������������ʧ��
}

static void TimerServiceTask(void *pv) {
    TimerCommand_t command;
    uint32_t ticksToWait;
    while (1) {
        //������Ҫ�����ȴ���ʱ��
        if (activeTimerListHead == NULL) {
            // û�л�Ķ�ʱ���������ڵȴ�����
            ticksToWait = MY_RTOS_MAX_DELAY;
        } else {
            uint64_t nextExpiryTime = activeTimerListHead->expiryTime;
            uint64_t currentTime = MyRTOS_GetTick();
            if (nextExpiryTime <= currentTime) {
                ticksToWait = 0; // �Ѿ����ڣ������������ȴ�
            } else {
                ticksToWait = nextExpiryTime - currentTime;
            }
        }
        //�����ȴ�������У���ʱʱ��Ϊ ticksToWait
        if (Queue_Receive(timerCommandQueue, &command, ticksToWait)) {
            if (command.timer == NULL) continue;
            // A. �յ����������������
            switch (command.command) {
                case TIMER_CMD_START:
                    // �����ʱ�����ڻ�б����Ƴ������¼���
                    if (command.timer->active) {
                        removeTimerFromActiveList(command.timer);
                    }
                    command.timer->active = 1;
                    command.timer->expiryTime = MyRTOS_GetTick() + command.timer->initialDelay;
                    insertTimerIntoActiveList(command.timer);
                    break;

                case TIMER_CMD_STOP:
                    if (command.timer->active) {
                        removeTimerFromActiveList(command.timer);
                    }
                    break;

                case TIMER_CMD_DELETE:
                    if (command.timer->active) {
                        removeTimerFromActiveList(command.timer);
                    }
                    rtos_free(command.timer); // �ͷŶ�ʱ�����ƿ��ڴ�
                    break;
            }
        } else {
            // B. �ȴ���ʱ����ζ���ж�ʱ��������
            // DBG_PRINTF("Timer Service Task: Timer expired\n");
            processExpiredTimers();
        }
    }
}

static void insertTimerIntoActiveList(Timer_t *timerToInsert) {
    if (activeTimerListHead == NULL || timerToInsert->expiryTime < activeTimerListHead->expiryTime) {
        timerToInsert->pNext = activeTimerListHead;
        activeTimerListHead = timerToInsert;
    } else {
        Timer_t *iterator = activeTimerListHead;
        while (iterator->pNext != NULL && iterator->pNext->expiryTime < timerToInsert->expiryTime) {
            iterator = iterator->pNext;
        }
        timerToInsert->pNext = iterator->pNext;
        iterator->pNext = timerToInsert;
    }
}

static void removeTimerFromActiveList(Timer_t *timerToRemove) {
    if (activeTimerListHead == timerToRemove) {
        // �Ƴ�����ͷ�ڵ�
        activeTimerListHead = timerToRemove->pNext;
    } else {
        Timer_t *iterator = activeTimerListHead;
        while (iterator != NULL && iterator->pNext != timerToRemove) {
            iterator = iterator->pNext;
        }
        if (iterator != NULL) {
            iterator->pNext = timerToRemove->pNext;
        }
    }
    // ����ָ�벢���Ϊ�ǻ
    timerToRemove->pNext = NULL;
    timerToRemove->active = 0;
}

static void processExpiredTimers(void) {
    uint64_t currentTime = MyRTOS_GetTick();

    while (activeTimerListHead != NULL && activeTimerListHead->expiryTime <= currentTime) {
        Timer_t *expiredTimer = activeTimerListHead;

        activeTimerListHead = expiredTimer->pNext;
        expiredTimer->pNext = NULL;
        expiredTimer->active = 0;

        if (expiredTimer->callback) {
            expiredTimer->callback(expiredTimer);
        }

        if (expiredTimer->period > 0) {
            // ���ּ��㷽ʽ���Դ������񱻳�ʱ���������´��������ڵ������
            // ����ȷ����һ�λ���ʱ������δ����ĳ��ʱ�̣�����������������
            uint64_t newExpiryTime = expiredTimer->expiryTime + expiredTimer->period;
            // ������������ʱ����Ȼ�ڹ�ȥ��˵��ϵͳ�ӳٷǳ����أ�
            // ����˲�ֹһ�����ڡ�������Ҫ������ʱ�� ׷��
            if (newExpiryTime <= currentTime) {
                // �������˶��ٸ�����
                uint64_t missed_periods = (currentTime - expiredTimer->expiryTime) / expiredTimer->period;
                // ������ʱ��ֱ���ƽ���δ��
                newExpiryTime = expiredTimer->expiryTime + (missed_periods + 1) * expiredTimer->period;
            }
            expiredTimer->expiryTime = newExpiryTime;
            insertTimerIntoActiveList(expiredTimer);
            expiredTimer->active = 1;
        }
    }
}


//============== ������ =============
MutexHandle_t Mutex_Create(void) {
    Mutex_t *mutex = rtos_malloc(sizeof(Mutex_t));
    if (mutex != NULL) {
        mutex->locked = 0;
        mutex->owner = (uint32_t) -1;
        mutex->waiting_mask = 0;
        mutex->owner_tcb = NULL;
        mutex->next_held_mutex = NULL;
    }
    return mutex;
}

void Mutex_Lock(MutexHandle_t mutex) {
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

void Mutex_Unlock(MutexHandle_t mutex) {
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


//=========== Interrupt Handlers ============

// �ҵ����滻ԭ���� SysTick_Handler ����
void SysTick_Handler(void) {
    uint32_t primask_status;
    systemTickCount++;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    // ��ȡ��ǰʱ���
    const uint64_t current_tick = systemTickCount;
    // ѭ���������е��ڵ�����
    while (delayedTaskListHead != NULL && delayedTaskListHead->delay <= current_tick) {
        Task_t *taskToWake = delayedTaskListHead;
        //����ʱ�������Ƴ� (�����Ƴ�ͷ��)
        delayedTaskListHead = taskToWake->pNextReady;
        if (delayedTaskListHead != NULL) {
            delayedTaskListHead->pPrevReady = NULL;
        }
        taskToWake->pNextReady = NULL;
        taskToWake->pPrevReady = NULL;
        //����ǳ�ʱ���������񣬻���Ҫ���¼��ȴ��б��Ƴ�
        if (taskToWake->state == TASK_STATE_BLOCKED && taskToWake->eventObject != NULL) {
            Queue_t *pQueue = (Queue_t *) taskToWake->eventObject;
            removeTaskFromEventList(&pQueue->sendWaitList, taskToWake);
            removeTaskFromEventList(&pQueue->receiveWaitList, taskToWake);
        }
        //�������ƻؾ����б�
        addTaskToReadyList(taskToWake);
    }

    MY_RTOS_YIELD();


    MY_RTOS_EXIT_CRITICAL(primask_status);
}

__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        " mrs r0, psp                       \n"
        " isb                               \n"

        " ldr r2, =currentTask              \n" /* r2 = &currentTask */
        " ldr r3, [r2]                      \n" /* r3 = currentTask */
        " cbz r3, 1f                        \n" /* ��� currentTask Ϊ NULL (�״ε���)���򲻱��� */

        /* ���� FPU ������ (S16-S31) */
        " tst lr, #0x10                     \n" /* ���� EXC_RETURN �� bit 4, 0��ʾʹ����FPU */
        " it eq                             \n" /* ��� bit 4 Ϊ0 */
        " vstmdbeq r0!, {s16-s31}           \n" /* ���� S16-S31 ��ջ�� */

        /* ����ͨ�üĴ��� (R4-R11) �� EXC_RETURN */
        " mov r1, lr                        \n" /* �� EXC_RETURN ֵ���� r1 */
        " stmdb r0!, {r1, r4-r11}           \n" /* ���� EXC_RETURN, R4-R11 */

        " str r0, [r3]                      \n" /* �����µ�ջ���� currentTask->sp */

        "1:                                 \n"
        " cpsid i                           \n" /* ���жϣ����� schedule_next_task */
        " bl schedule_next_task             \n" /* r0 = nextTask->sp (schedule_next_task�ķ���ֵ) */
        " cpsie i                           \n" /* ���ж� */

        " ldr r2, =currentTask              \n"
        " ldr r2, [r2]                      \n" /* r2 = currentTask (�µ�) */
        " ldr r0, [r2]                      \n" /* r0 = currentTask->sp (�µ�ջ��) */

        /* �ָ�ͨ�üĴ��� (R4-R11) �� EXC_RETURN */
        " ldmia r0!, {r1, r4-r11}           \n" /* �ָ� EXC_RETURN �� r1, R4-R11 */
        " mov lr, r1                        \n" /* ���� LR �Ĵ��� */

        /* �ָ� FPU ������ */
        " tst lr, #0x10                     \n" /* �ٴβ��� EXC_RETURN �� bit 4 */
        " it eq                             \n"
        " vldmiaeq r0!, {s16-s31}           \n" /* �ָ� S16-S31 */

        " msr psp, r0                       \n"
        " isb                               \n"

        " bx lr                             \n"
    );
}

void HardFault_Handler(void) {
    __disable_irq();

    uint32_t stacked_pc = 0;
    uint32_t sp = 0;
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t bfar = SCB->BFAR;

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

    if (cfsr & (1UL << 15)) {
        DBG_PRINTF("Bus Fault Address: 0x%08lX\n", bfar);
    }

    DBG_PRINTF("LR: 0x%08lX, SP: 0x%08lX, Stacked PC: 0x%08lX\n", lr, sp, stacked_pc);

    if (cfsr & SCB_CFSR_IACCVIOL_Msk)
        DBG_PRINTF("Fault: Instruction Access Violation\n");
    if (cfsr & SCB_CFSR_DACCVIOL_Msk)
        DBG_PRINTF("Fault: Data Access Violation\n");
    if (cfsr & SCB_CFSR_MUNSTKERR_Msk)
        DBG_PRINTF("Fault: Unstacking Error\n");
    if (cfsr & SCB_CFSR_MSTKERR_Msk)
        DBG_PRINTF("Fault: Stacking Error\n");
    if (cfsr & SCB_CFSR_INVSTATE_Msk)
        DBG_PRINTF("Fault: Invalid State\n");
    if (cfsr & SCB_CFSR_UNDEFINSTR_Msk)
        DBG_PRINTF("Fault: Undefined Instruction\n");
    if (cfsr & SCB_CFSR_IBUSERR_Msk)
        DBG_PRINTF("Fault: Instruction Bus Error\n");
    if (cfsr & SCB_CFSR_PRECISERR_Msk)
        DBG_PRINTF("Fault: Precise Data Bus Error\n");

    while (1);
}


__attribute__((naked)) void SVC_Handler(void) {
    __asm volatile (
        " ldr r0, =currentTask      \n"
        " ldr r0, [r0]              \n" // r0 = currentTask
        " ldr r0, [r0]              \n" // r0 = currentTask->sp

        " ldmia r0!, {r1, r4-r11}   \n" // �ָ� EXC_RETURN �� r1, �� R4-R11
        " mov lr, r1                \n" // �� EXC_RETURN д�� LR

        " tst lr, #0x10             \n" // ����Ƿ���Ҫ�ָ� FPU ������
        " it eq                     \n"
        " vldmiaeq r0!, {s16-s31}   \n" // �ָ� S16-S31

        " msr psp, r0               \n" // �ָ� PSP
        " isb                       \n"

        " movs r0, #2               \n" // Thread+PSP
        " msr control, r0           \n"
        " isb                       \n"

        " bx lr                     \n" // ʹ�ûָ��� EXC_RETURN ֵ����
    );
}
