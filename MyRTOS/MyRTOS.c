//
// Created by XiaoXiu on 8/22/2025.
//
#include "MyRTOS.h"
#include <stdlib.h>
#include <string.h>

// ������ƿ� (TCB)
//Ϊ�˱��ֻ�����ļ����� sp �����ǽṹ��ĵ�һ����Ա
typedef struct Task_t {
    uint32_t *sp;

    void (*func)(void *); // ������
    void *param; // �������
    uint32_t delay; // ��ʱ
    volatile uint32_t notification;
    volatile uint8_t is_waiting_notification;
    volatile TaskState_t state; // ����״̬
    uint32_t taskId; // ����ID
    uint32_t *stack_base; // ջ����ַ,����free
    uint8_t priority; //�������ȼ�
    struct Task_t *pNextTask; //����������������
    struct Task_t *pNextReady; //���ھ�������ʱ����
    struct Task_t *pPrevReady; //����˫������,����ɾ�� O(1)���Ӷ�
    struct Mutex_t *held_mutexes_head;
    void *eventObject; // ָ�����ڵȴ����ں˶���
    void *eventData; // ���ڴ������¼���ص�����ָ�� (����Ϣ��Դ/Ŀ�ĵ�ַ)
    struct Task_t *pNextEvent; // ���ڹ����ں˶���ĵȴ���������
} Task_t;

// �������ṹ��
// ��ġ���ȷ�� Mutex_t ����
typedef struct Mutex_t {
    volatile int locked;
    volatile uint32_t owner;
    volatile uint32_t waiting_mask;
    Task_t *owner_tcb;
    struct Mutex_t *next_held_mutex;
} Mutex_t;

// ��Ϣ���нṹ��
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

// ��ʱ���ṹ��
typedef struct Timer_t {
    TimerCallback_t callback; // ��ʱ������ʱִ�еĻص�����
    void *arg; // ���ݸ��ص������Ķ������
    uint32_t initialDelay; // �״δ�����ʱ (in ticks)
    uint32_t period; // ���� (in ticks), ���Ϊ0���ǵ��ζ�ʱ��
    volatile uint32_t expiryTime; // ��һ�ε���ʱ�ľ���ϵͳtick
    struct Timer_t *pNext; // ���ڹ������ʱ������
    volatile uint8_t active; // ��ʱ���Ƿ��ڻ״̬ (0:inactive, 1:active)
} Timer_t;

// ��ʱ��������������
typedef enum {
    TIMER_CMD_START,
    TIMER_CMD_STOP,
    TIMER_CMD_DELETE
} TimerCommandType_t;

typedef struct {
    TimerCommandType_t command;
    TimerHandle_t timer;
} TimerCommand_t;

// �ڴ��ͷ���ṹ (�������ڴ����ģ��)
typedef struct BlockLink_t {
    struct BlockLink_t *nextFreeBlock; /* ָ����������һ�������ڴ�� */
    size_t blockSize; /* ��ǰ�ڴ��Ĵ�С(����ͷ��), ���λ���������� */
} BlockLink_t;


/*===========================================================================*/
/* �ں�ȫ�ֱ��� (Kernel Global Variables)                                 */
/*===========================================================================*/

// �������
static Task_t *allTaskListHead = NULL;
static Task_t *volatile currentTask = NULL;
static Task_t *idleTask = NULL;
static uint32_t nextTaskId = 0;
static Task_t *readyTaskLists[MY_RTOS_MAX_PRIORITIES];
static Task_t *delayedTaskListHead = NULL;
static Task_t *tasksToDeleteList = NULL;
static volatile uint32_t topReadyPriority = 0;

// ϵͳTick
static volatile uint64_t systemTickCount = 0;

// �����ʱ��
static TaskHandle_t timerServiceTaskHandle = NULL;
static QueueHandle_t timerCommandQueue = NULL;
static Timer_t *activeTimerListHead = NULL;


/*===========================================================================*/
/* �ڲ�����ǰ������ (Internal Function Forward Declarations)              */
/*===========================================================================*/

static void *rtos_malloc(size_t wantedSize);

static void rtos_free(void *pv);

static void addTaskToReadyList(Task_t *task);

static void removeTaskFromList(Task_t **ppListHead, Task_t *pTaskToRemove);

void *schedule_next_task(void);

static void MyRTOS_Idle_Task(void *pv);

static void TimerServiceTask(void *pv);

static void Mutex_Init(MutexHandle_t mutex);

/*===========================================================================*/
/* ��̬�ڴ���� (Heap Management)                                 */
/*===========================================================================*/

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
    for (iterator = &start; iterator->nextFreeBlock < blockToInsert; iterator = iterator->nextFreeBlock) {
        /* ʲô����������ֻ���ҵ�λ�� */
    }

    /* �����¿��Ƿ��������ָ��Ŀ������������� */
    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        /* ���ڣ��ϲ����� */
        iterator->blockSize += blockToInsert->blockSize;
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
static void *rtos_malloc(size_t wantedSize) {
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


/*===========================================================================*/
/* ���������ϵͳ���� (Task & System Core)                         */
/*===========================================================================*/

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
    uint64_t tick_value;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    tick_value = systemTickCount;
    MY_RTOS_EXIT_CRITICAL(primask_status);
    return tick_value;
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

static void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // ����͹���ģʽ���ȴ��ж�
    }
}

Task_t *Task_Create(void (*func)(void *), uint16_t stack_size, void *param, uint8_t priority) {
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
    // t->state �� prvAddTaskToReadyList ������
    t->taskId = nextTaskId++;
    t->stack_base = stack;
    t->pNextTask = NULL;
    t->pNextReady = NULL;
    t->pPrevReady = NULL;
    t->priority = priority;

    uint32_t* sp = (uint32_t*)((uint8_t*)stack + stack_size_bytes);
    sp = (uint32_t *) (((uintptr_t) sp) & ~0x7u);
    sp -= 16;
    // ������ memset(sp, 0, 16 * sizeof(uint32_t)) �����㣬���Ǳ���

    /* Ӳ���Զ������ջ֡ (R0-R3, R12, LR, PC, xPSR) */
    sp -= 8;
    sp[0] = (uint32_t) param; // R0 (����)
    sp[1] = 0x01010101; // R1
    sp[2] = 0x02020202; // R2
    sp[3] = 0x03030303; // R3
    sp[4] = 0x12121212; // R12
    sp[5] = 0; // LR (���񷵻ص�ַ����Ϊ0��������ɱ����)
    sp[6] = ((uint32_t) func) | 1u; // PC (��ڵ�)
    sp[7] = 0x01000000; // xPSR (Thumb bit must be 1)

    /* ����ֶ������ͨ�üĴ��� (R4 - R11) �� EXC_RETURN */
    sp -= 9;
    sp[0] = 0xFFFFFFFD; // EXC_RETURN: ָʾ����ʱ�ָ�FPU������
    sp[1] = 0x04040404; // R4
    sp[2] = 0x05050505; // R5
    sp[3] = 0x06060606; // R6
    sp[4] = 0x07070707; // R7
    sp[5] = 0x08080808; // R8
    sp[6] = 0x09090909; // R9
    sp[7] = 0x0A0A0A0A; // R10
    sp[8] = 0x0B0B0B0B; // R11

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
               &stack[stack_size_bytes - 1], t->sp);
    return t;
}

/* In MyRTOS.c */
int Task_Delete(TaskHandle_t task_h) {
    if (task_h == idleTask) return -1;

    Task_t *task_to_delete;
    if (task_h == NULL) {
        task_to_delete = currentTask;
    } else {
        task_to_delete = task_h;
    }

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    //�������ڵ��κλ�������Ƴ�
    switch (task_to_delete->state) {
        case TASK_STATE_READY:
            removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
            break;
        case TASK_STATE_DELAYED:
            removeTaskFromList(&delayedTaskListHead, task_to_delete);
            break;
        case TASK_STATE_BLOCKED:
            // TODO: ���������������ĳ���¼��ϣ�Ҳ��Ҫ�������¼��ȴ��б����Ƴ�
            // ���磬��� eventObject != NULL������Ҫ���ݶ������ʹ���
            break;
        default:
            break;
    }

    // 2. �� allTaskListHead ���Ƴ�
    Task_t *prev = NULL;
    Task_t *curr = allTaskListHead;
    while (curr != NULL && curr != task_to_delete) {
        prev = curr;
        curr = curr->pNextTask;
    }
    if (curr != NULL) {
        if (prev == NULL) allTaskListHead = curr->pNextTask;
        else prev->pNextTask = curr->pNextTask;
    }

    task_to_delete->state = TASK_STATE_UNUSED;

    // 3. �����ɾ���������񣬿���ֱ���ͷ��ڴ�
    if (task_to_delete != currentTask) {
        rtos_free(task_to_delete->stack_base);
        rtos_free(task_to_delete);
    } else {
        // 4. �����������ɱ����������ɾ���б�
        task_to_delete->pNextTask = tasksToDeleteList;
        tasksToDeleteList = task_to_delete;

        // 5. �����������ȣ��л���ȥ
        MY_RTOS_YIELD();
    }

    MY_RTOS_EXIT_CRITICAL(primask_status);

    // ע�⣺�������ɱ��������Զ����ִ�е�����
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
}

// �������������
void *schedule_next_task(void) {
    Task_t *nextTaskToRun = NULL;

    if (topReadyPriority == 0) {
        // û�о�������ֻ�����п�������
        nextTaskToRun = idleTask;
    } else {
        // AI����ħ������: ʹ�� CLZ (Count Leading Zeros) ָ������ҵ���ߵ���λ
        uint32_t highestPriority = 31 - __CLZ(topReadyPriority);

        // �Ӹ����ȼ��ľ�������ͷȡ������
        nextTaskToRun = readyTaskLists[highestPriority];

        // ʵ��ͬ���ȼ��������ѯ (Round-Robin)
        if (nextTaskToRun != NULL && nextTaskToRun->pNextReady != NULL) {
            readyTaskLists[highestPriority] = nextTaskToRun->pNextReady;
            nextTaskToRun->pNextReady->pPrevReady = NULL;

            Task_t *pLast = readyTaskLists[highestPriority];
            while (pLast->pNextReady != NULL) pLast = pLast->pNextReady;
            pLast->pNextReady = nextTaskToRun;
            nextTaskToRun->pPrevReady = pLast;
            nextTaskToRun->pNextReady = NULL;
        }
    }

    currentTask = nextTaskToRun;

    if (currentTask == NULL) {
        // Should not happen after idle task is created
        return NULL;
    }

    return currentTask->sp;
}

void Task_StartScheduler(void) {
    // ������������ ���ȼ���� (0)
    idleTask = Task_Create(MyRTOS_Idle_Task, 64, NULL, 0);
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

    DBG_PRINTF("Starting scheduler...\n");

    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0x00);
    if (SysTick_Config(SystemCoreClock / MY_RTOS_TICK_RATE_HZ)) {
        DBG_PRINTF("Error: SysTick_Config failed\n");
        while (1);
    }

    // ��һ�ε���,�ֶ�ѡ����һ������
    schedule_next_task();

    __asm volatile("svc 0");
    for (;;);
}

int Task_Notify(TaskHandle_t task_h) {
    uint32_t primask_status;
    int trigger_yield = 0; // �Ƿ���Ҫ��ռ���ȵı�־

    MY_RTOS_ENTER_CRITICAL(primask_status);

    // ��������Ƿ�ȷʵ�ڵȴ�֪ͨ
    if (task_h->is_waiting_notification && task_h->state == TASK_STATE_BLOCKED) {
        task_h->is_waiting_notification = 0;
        addTaskToReadyList(task_h);
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
}


TaskState_t Task_GetState(const TaskHandle_t task_h) {
    // ǿ������ת������Ϊ��.c�ļ��ڲ�  TaskHandle_t ���� Task_t*
    Task_t *task = task_h;
    if (task == NULL) {
        return TASK_STATE_UNUSED;
    }
    return task->state;
}

uint8_t Task_GetPriority(const TaskHandle_t task_h) {
    Task_t *task = task_h;
    if (task == NULL) {
        // ����һ����Ч�����ȼ�
        return (uint8_t) -1;
    }
    return task->priority;
}

TaskHandle_t Task_GetCurrentTaskHandle(void) {
    return currentTask;
}


/*===========================================================================*/
/* ��Ϣ���� (Queue Management)                                    */
/*===========================================================================*/

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
        taskToInsert->pNextEvent = *listHead;
        *listHead = taskToInsert;
    } else {
        Task_t *iterator = *listHead;
        while (iterator->pNextEvent != NULL && iterator->pNextEvent->priority > taskToInsert->priority) {
            iterator = iterator->pNextEvent;
        }
        taskToInsert->pNextEvent = iterator->pNextEvent;
        iterator->pNextEvent = taskToInsert;
    }
}

QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize) {
    if (length == 0 || itemSize == 0) return NULL;

    Queue_t *queue = rtos_malloc(sizeof(Queue_t));
    if (queue == NULL) return NULL;

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
    if (pQueue == NULL) return 0;

    uint32_t primask_status;

    while (1) {
        MY_RTOS_ENTER_CRITICAL(primask_status);

        if (pQueue->receiveWaitList != NULL) {
            Task_t *taskToWake = pQueue->receiveWaitList;
            removeTaskFromEventList(&pQueue->receiveWaitList, taskToWake);
            if (taskToWake->delay > 0) {
                removeTaskFromList(&delayedTaskListHead, taskToWake);
                taskToWake->delay = 0;
            }
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            taskToWake->eventObject = NULL;
            taskToWake->eventData = NULL;
            addTaskToReadyList(taskToWake);
            if (taskToWake->priority > currentTask->priority)
                MY_RTOS_YIELD();
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1;
        }

        if (pQueue->waitingCount < pQueue->length) {
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1;
        }

        if (block_ticks == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0;
        }

        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        currentTask->eventObject = pQueue;
        addTaskToPrioritySortedList(&pQueue->sendWaitList, currentTask);
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = block_ticks;
            currentTask->pNextReady = delayedTaskListHead;
            currentTask->pPrevReady = NULL;
            if (delayedTaskListHead != NULL) delayedTaskListHead->pPrevReady = currentTask;
            delayedTaskListHead = currentTask;
        }

        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);

        if (currentTask->eventObject == NULL) {
            continue;
        }

        MY_RTOS_ENTER_CRITICAL(primask_status);
        removeTaskFromEventList(&pQueue->sendWaitList, currentTask);
        currentTask->eventObject = NULL;
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
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;

            if (pQueue->sendWaitList != NULL) {
                Task_t *taskToWake = pQueue->sendWaitList;
                removeTaskFromEventList(&pQueue->sendWaitList, taskToWake);
                if (taskToWake->delay > 0) {
                    removeTaskFromList(&delayedTaskListHead, taskToWake);
                    taskToWake->delay = 0;
                }
                taskToWake->eventObject = NULL;
                addTaskToReadyList(taskToWake);
                if (taskToWake->priority > currentTask->priority)
                    MY_RTOS_YIELD();
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1;
        }

        if (block_ticks == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0;
        }

        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        currentTask->eventObject = pQueue;
        currentTask->eventData = buffer;
        addTaskToPrioritySortedList(&pQueue->receiveWaitList, currentTask);
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = block_ticks;
            currentTask->pNextReady = delayedTaskListHead;
            currentTask->pPrevReady = NULL;
            if (delayedTaskListHead != NULL) delayedTaskListHead->pPrevReady = currentTask;
            delayedTaskListHead = currentTask;
        }

        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);

        if (currentTask->eventObject == NULL) return 1;

        MY_RTOS_ENTER_CRITICAL(primask_status);
        removeTaskFromEventList(&pQueue->receiveWaitList, currentTask);
        currentTask->eventObject = NULL;
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return 0;
    }
}


/*===========================================================================*/
/* �����ʱ�� (Software Timer Management)                         */
/*===========================================================================*/

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
            expiredTimer->expiryTime += expiredTimer->period;
            insertTimerIntoActiveList(expiredTimer);
            expiredTimer->active = 1;
        }
    }
}

static void TimerServiceTask(void *pv) {
    TimerCommand_t command;
    uint32_t ticksToWait;
    while (1) {
        if (activeTimerListHead == NULL) {
            ticksToWait = MY_RTOS_MAX_DELAY;
        } else {
            uint64_t nextExpiryTime = activeTimerListHead->expiryTime;
            uint64_t currentTime = MyRTOS_GetTick();
            ticksToWait = (nextExpiryTime <= currentTime) ? 0 : (nextExpiryTime - currentTime);
        }

        if (Queue_Receive(timerCommandQueue, &command, ticksToWait)) {
            if (command.timer == NULL) continue;
            switch (command.command) {
                case TIMER_CMD_START:
                    if (command.timer->active) removeTimerFromActiveList(command.timer);
                    command.timer->active = 1;
                    command.timer->expiryTime = MyRTOS_GetTick() + command.timer->initialDelay;
                    insertTimerIntoActiveList(command.timer);
                    break;
                case TIMER_CMD_STOP:
                    if (command.timer->active) removeTimerFromActiveList(command.timer);
                    break;
                case TIMER_CMD_DELETE:
                    if (command.timer->active) removeTimerFromActiveList(command.timer);
                    rtos_free(command.timer);
                    break;
            }
        } else {
            processExpiredTimers();
        }
    }
}

TimerHandle_t Timer_Create(uint32_t delay, uint32_t period, TimerCallback_t callback, void *arg) {
    TimerHandle_t timer = rtos_malloc(sizeof(Timer_t));
    if (timer) {
        timer->callback = callback;
        timer->arg = arg;
        timer->initialDelay = delay;
        timer->period = period;
        timer->active = 0;
        timer->pNext = NULL;
    }
    return timer;
}

static int sendCommandToTimerTask(TimerHandle_t timer, TimerCommandType_t cmd, uint32_t block) {
    if (timerCommandQueue == NULL || timer == NULL) return -1;
    TimerCommand_t command = {.command = cmd, .timer = timer};
    return Queue_Send(timerCommandQueue, &command, block) ? 0 : -1;
}

int Timer_Start(TimerHandle_t timer) {
    return sendCommandToTimerTask(timer, TIMER_CMD_START, 0);
}

int Timer_Stop(TimerHandle_t timer) {
    return sendCommandToTimerTask(timer, TIMER_CMD_STOP, 0);
}

int Timer_Delete(TimerHandle_t timer) {
    return sendCommandToTimerTask(timer, TIMER_CMD_DELETE, 0);
}


/*===========================================================================*/
/* ������ (Mutex Management)                                      */
/*===========================================================================*/

static void Mutex_Init(MutexHandle_t mutex) {
    if (mutex == NULL) return;
    mutex->locked = 0;
    mutex->owner = (uint32_t) -1;
    mutex->waiting_mask = 0;
    mutex->owner_tcb = NULL;
    mutex->next_held_mutex = NULL;
}

/**
 * @brief ����һ��������.
 */
MutexHandle_t Mutex_Create(void) {
    // 1. Ϊ�������ṹ�嶯̬�����ڴ�
    MutexHandle_t mutex = (MutexHandle_t) rtos_malloc(sizeof(Mutex_t));

    // 2. ����ڴ��Ƿ����ɹ�
    if (mutex != NULL) {
        // 3. �����ڲ���ʼ�����������ó�ʼֵ
        Mutex_Init(mutex);
    }

    // 4. ���ؾ�� (�������ʧ�ܣ����ﷵ�صľ��� NULL)
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


/*===========================================================================*/
/* Ӳ����ֲ�� (Porting Layer - Interrupt Handlers)                */
/*===========================================================================*/

//���䶨��
#ifndef SCB_CFSR_IACCVIOL_Msk
#define SCB_CFSR_IACCVIOL_Msk     (1UL << 0)
#define SCB_CFSR_DACCVIOL_Msk     (1UL << 1)
#define SCB_CFSR_MUNSTKERR_Msk    (1UL << 3)
#define SCB_CFSR_MSTKERR_Msk      (1UL << 4)
#define SCB_CFSR_UNDEFINSTR_Msk   (1UL << 16)
#define SCB_CFSR_INVSTATE_Msk     (1UL << 17)
#endif

void SysTick_Handler(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    systemTickCount++;

    Task_t *p = delayedTaskListHead;
    Task_t *pNext = NULL;

    while (p != NULL) {
        pNext = p->pNextReady;
        if (p->delay > 0) p->delay--;
        if (p->delay == 0) {
            removeTaskFromList(&delayedTaskListHead, p);
            // ����������¼���ʱ����Ҫ������¼��ȴ��б����Ƴ�
            if (p->eventObject != NULL) {
                Queue_t *pQueue = (Queue_t *) p->eventObject;
                // ������Ҫ�ж����ĸ��ȴ��б��򻯴������趼���
                removeTaskFromEventList(&pQueue->sendWaitList, p);
                removeTaskFromEventList(&pQueue->receiveWaitList, p);
            }
            addTaskToReadyList(p);
        }
        p = pNext;
    }

    // ÿ��SysTick���������, �Դ�����ܷ�������ռ
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
        " it eq                             \n" /* <<<<<<< �޸�: �ָ��������ؼ�ָ�� */
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
        " it eq                             \n" /* <<<<<<< �޸�: �ָ��������ؼ�ָ�� */
        " vldmiaeq r0!, {s16-s31}           \n" /* �ָ� S16-S31 */

        " msr psp, r0                       \n"
        " isb                               \n"
        " bx lr                             \n"
    );
}


__attribute__((naked)) void SVC_Handler(void) {
    __asm volatile (
        " ldr r0, =currentTask      \n"
        " ldr r0, [r0]              \n" // r0 = currentTask
        " ldr r0, [r0]              \n" // r0 = currentTask->sp

        /* �ָ�ͨ�üĴ��� (R4-R11) �� EXC_RETURN */
        " ldmia r0!, {r1, r4-r11}   \n"
        " mov lr, r1                \n"

        /* �ָ� FPU ������ */
        " tst lr, #0x10             \n" // ����Ƿ���Ҫ�ָ� FPU ������
        " it eq                     \n" // <<<<<<< �޸�: �ָ��������ؼ�ָ��
        " vldmiaeq r0!, {s16-s31}   \n" // �ָ� S16-S31

        " msr psp, r0               \n" // �ָ� PSP
        " isb                       \n"

        " movs r0, #2               \n" // Thread+PSP
        " msr control, r0           \n"
        " isb                       \n"

        " bx lr                     \n" // ʹ�ûָ��� EXC_RETURN ֵ����
    );
}

void HardFault_Handler(void) {
    __disable_irq();

    uint32_t sp;
    __asm volatile ("mrs %0, psp" : "=r"(sp));

    uint32_t stacked_pc = ((uint32_t *) sp)[6];
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;

    DBG_PRINTF("\n!!! Hard Fault !!!\n");
    DBG_PRINTF("Current Task ID: %d\n", currentTask ? currentTask->taskId : -1);
    DBG_PRINTF("SP: 0x%08lX, Stacked PC: 0x%08lX\n", sp, stacked_pc);
    DBG_PRINTF("CFSR: 0x%08lX, HFSR: 0x%08lX\n", cfsr, hfsr);

    if (cfsr & SCB_CFSR_IACCVIOL_Msk)
        DBG_PRINTF("Fault: Instruction Access Violation\n");
    if (cfsr & SCB_CFSR_DACCVIOL_Msk)
        DBG_PRINTF("Fault: Data Access Violation\n");
    if (cfsr & SCB_CFSR_UNDEFINSTR_Msk)
        DBG_PRINTF("Fault: Undefined Instruction\n");
    if (cfsr & SCB_CFSR_INVSTATE_Msk)
        DBG_PRINTF("Fault: Invalid State\n");

    while (1);
}
