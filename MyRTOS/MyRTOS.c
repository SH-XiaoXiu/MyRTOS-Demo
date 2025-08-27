//
// Created by XiaoXiu on 8/22/2025.
//
#include "MyRTOS.h"
#include <stdlib.h>
#include <string.h>

// 任务控制块 (TCB)
//为了保持汇编代码的兼容性 sp 必须是结构体的第一个成员
typedef struct Task_t {
    uint32_t *sp;

    void (*func)(void *); // 任务函数
    void *param; // 任务参数
    uint32_t delay; // 延时
    volatile uint32_t notification;
    volatile uint8_t is_waiting_notification;
    volatile TaskState_t state; // 任务状态
    uint32_t taskId; // 任务ID
    uint32_t *stack_base; // 栈基地址,用于free
    uint8_t priority; //任务优先级
    struct Task_t *pNextTask; //用于所有任务链表
    struct Task_t *pNextReady; //用于就绪或延时链表
    struct Task_t *pPrevReady; //用于双向链表,方便删除 O(1)复杂度
    struct Mutex_t *held_mutexes_head;
    void *eventObject; // 指向正在等待的内核对象
    void *eventData; // 用于传递与事件相关的数据指针 (如消息的源/目的地址)
    struct Task_t *pNextEvent; // 用于构建内核对象的等待任务链表
} Task_t;

// 互斥锁结构体
// 你的、正确的 Mutex_t 定义
typedef struct Mutex_t {
    volatile int locked;
    volatile uint32_t owner;
    volatile uint32_t waiting_mask;
    Task_t *owner_tcb;
    struct Mutex_t *next_held_mutex;
} Mutex_t;

// 消息队列结构体
typedef struct Queue_t {
    uint8_t *storage; // 指向队列存储区的指针
    uint32_t length; // 队列最大能容纳的消息数
    uint32_t itemSize; // 每个消息的大小
    volatile uint32_t waitingCount; // 当前队列中的消息数
    uint8_t *writePtr; // 下一个要写入数据的位置
    uint8_t *readPtr; // 下一个要读取数据的位置
    // 等待列表 (将按任务优先级排序)
    Task_t *sendWaitList;
    Task_t *receiveWaitList;
} Queue_t;

// 定时器结构体
typedef struct Timer_t {
    TimerCallback_t callback; // 定时器到期时执行的回调函数
    void *arg; // 传递给回调函数的额外参数
    uint32_t initialDelay; // 首次触发延时 (in ticks)
    uint32_t period; // 周期 (in ticks), 如果为0则是单次定时器
    volatile uint32_t expiryTime; // 下一次到期时的绝对系统tick
    struct Timer_t *pNext; // 用于构建活动定时器链表
    volatile uint8_t active; // 定时器是否处于活动状态 (0:inactive, 1:active)
} Timer_t;

// 定时器服务任务命令
typedef enum {
    TIMER_CMD_START,
    TIMER_CMD_STOP,
    TIMER_CMD_DELETE
} TimerCommandType_t;

typedef struct {
    TimerCommandType_t command;
    TimerHandle_t timer;
} TimerCommand_t;

// 内存块头部结构 (仅用于内存管理模块)
typedef struct BlockLink_t {
    struct BlockLink_t *nextFreeBlock; /* 指向链表中下一个空闲内存块 */
    size_t blockSize; /* 当前内存块的大小(包含头部), 最高位用作分配标记 */
} BlockLink_t;


/*===========================================================================*/
/* 内核全局变量 (Kernel Global Variables)                                 */
/*===========================================================================*/

// 任务管理
static Task_t *allTaskListHead = NULL;
static Task_t *volatile currentTask = NULL;
static Task_t *idleTask = NULL;
static uint32_t nextTaskId = 0;
static Task_t *readyTaskLists[MY_RTOS_MAX_PRIORITIES];
static Task_t *delayedTaskListHead = NULL;
static Task_t *tasksToDeleteList = NULL;
static volatile uint32_t topReadyPriority = 0;

// 系统Tick
static volatile uint64_t systemTickCount = 0;

// 软件定时器
static TaskHandle_t timerServiceTaskHandle = NULL;
static QueueHandle_t timerCommandQueue = NULL;
static Timer_t *activeTimerListHead = NULL;


/*===========================================================================*/
/* 内部函数前置声明 (Internal Function Forward Declarations)              */
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
/* 动态内存管理 (Heap Management)                                 */
/*===========================================================================*/

/* 内存块头部结构的大小 (已对齐) */
static const size_t heapStructSize = (sizeof(BlockLink_t) + (HEAP_BYTE_ALIGNMENT - 1)) & ~(
                                         (size_t) HEAP_BYTE_ALIGNMENT - 1);
/* 最小内存块大小，一个块必须能容纳分裂后的两个头部 */
#define HEAP_MINIMUM_BLOCK_SIZE    (heapStructSize * 2)

/* 静态内存池 */
static uint8_t rtos_memory_pool[RTOS_MEMORY_POOL_SIZE] __attribute__((aligned(HEAP_BYTE_ALIGNMENT)));
/* 空闲链表的起始和结束标记 */
static BlockLink_t start, *blockLinkEnd = NULL;
/* 剩余可用内存大小 */
static size_t freeBytesRemaining = 0U;
/* 用于标记内存块是否已被分配的标志位 */
static size_t blockAllocatedBit = 0;

/* 初始化堆内存 (仅在首次分配时调用) */
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

/* 将一个内存块插入到空闲链表中，并处理与相邻空闲块的合并 */
static void insertBlockIntoFreeList(BlockLink_t *blockToInsert) {
    BlockLink_t *iterator;
    uint8_t *puc;

    /* 遍历链表，找到可以插入新释放块的位置 (按地址排序) */
    for (iterator = &start; iterator->nextFreeBlock < blockToInsert; iterator = iterator->nextFreeBlock) {
        /* 什么都不用做，只是找到位置 */
    }

    /* 看看新块是否与迭代器指向的块在物理上相邻 */
    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        /* 相邻，合并它们 */
        iterator->blockSize += blockToInsert->blockSize;
        blockToInsert = iterator;
    } else {
        /* 不相邻，将新块链接到迭代器后面 */
        blockToInsert->nextFreeBlock = iterator->nextFreeBlock;
    }

    /* 看看新块是否与后面的块相邻 */
    puc = (uint8_t *) blockToInsert;
    if ((puc + blockToInsert->blockSize) == (uint8_t *) iterator->nextFreeBlock) {
        if (iterator->nextFreeBlock != blockLinkEnd) {
            /* 相邻，合并它们 */
            blockToInsert->blockSize += iterator->nextFreeBlock->blockSize;
            blockToInsert->nextFreeBlock = iterator->nextFreeBlock->nextFreeBlock;
        }
    }

    /* 如果第一步没有合并，那么迭代器的下一个节点需要指向新块 */
    if (iterator != blockToInsert) {
        iterator->nextFreeBlock = blockToInsert;
    }
}

/* 动态内存分配函数 */
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

/* 动态内存释放函数 */
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
/* 任务管理与系统核心 (Task & System Core)                         */
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

    // 标记该优先级有就绪任务了
    topReadyPriority |= (1UL << task->priority);

    // 将任务插入到对应优先级链表的尾部
    task->pNextReady = NULL;
    if (readyTaskLists[task->priority] == NULL) {
        // 这是该优先级的第一个任务
        readyTaskLists[task->priority] = task;
        task->pPrevReady = NULL;
    } else {
        // 找到链表尾部并插入
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

    // 更新前一个节点的 next 指针
    if (pTaskToRemove->pPrevReady != NULL) {
        pTaskToRemove->pPrevReady->pNextReady = pTaskToRemove->pNextReady;
    } else {
        // 移除的是头节点
        *ppListHead = pTaskToRemove->pNextReady;
    }

    // 更新后一个节点的 prev 指针
    if (pTaskToRemove->pNextReady != NULL) {
        pTaskToRemove->pNextReady->pPrevReady = pTaskToRemove->pPrevReady;
    }

    // 清理被移除节点的指针
    pTaskToRemove->pNextReady = NULL;
    pTaskToRemove->pPrevReady = NULL;

    // 如果任务是从就绪列表中移除，还需要检查是否需要清除优先级位图
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
        __WFI(); // 进入低功耗模式，等待中断
    }
}

Task_t *Task_Create(void (*func)(void *), uint16_t stack_size, void *param, uint8_t priority) {
    // 检查优先级是否有效
    if (priority >= MY_RTOS_MAX_PRIORITIES) {
        DBG_PRINTF("Error: Invalid task priority %u.\n", priority);
        return NULL;
    }

    //为任务控制块 TCB 分配内存
    Task_t *t = rtos_malloc(sizeof(Task_t));
    if (t == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for TCB.\n");
        return NULL;
    }

    //为任务栈分配内存
    uint32_t stack_size_bytes = stack_size * sizeof(uint32_t);
    uint32_t *stack = rtos_malloc(stack_size_bytes);
    if (stack == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for stack.\n");
        rtos_free(t);
        return NULL;
    }

    // 初始化TCB成员
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    // t->state 在 prvAddTaskToReadyList 中设置
    t->taskId = nextTaskId++;
    t->stack_base = stack;
    t->pNextTask = NULL;
    t->pNextReady = NULL;
    t->pPrevReady = NULL;
    t->priority = priority;

    uint32_t* sp = (uint32_t*)((uint8_t*)stack + stack_size_bytes);
    sp = (uint32_t *) (((uintptr_t) sp) & ~0x7u);
    sp -= 16;
    // 可以用 memset(sp, 0, 16 * sizeof(uint32_t)) 来清零，但非必须

    /* 硬件自动保存的栈帧 (R0-R3, R12, LR, PC, xPSR) */
    sp -= 8;
    sp[0] = (uint32_t) param; // R0 (参数)
    sp[1] = 0x01010101; // R1
    sp[2] = 0x02020202; // R2
    sp[3] = 0x03030303; // R3
    sp[4] = 0x12121212; // R12
    sp[5] = 0; // LR (任务返回地址，设为0或任务自杀函数)
    sp[6] = ((uint32_t) func) | 1u; // PC (入口点)
    sp[7] = 0x01000000; // xPSR (Thumb bit must be 1)

    /* 软件手动保存的通用寄存器 (R4 - R11) 和 EXC_RETURN */
    sp -= 9;
    sp[0] = 0xFFFFFFFD; // EXC_RETURN: 指示返回时恢复FPU上下文
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

    // 添加到所有任务链表
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

    //从它所在的任何活动链表中移除
    switch (task_to_delete->state) {
        case TASK_STATE_READY:
            removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
            break;
        case TASK_STATE_DELAYED:
            removeTaskFromList(&delayedTaskListHead, task_to_delete);
            break;
        case TASK_STATE_BLOCKED:
            // TODO: 如果任务正阻塞在某个事件上，也需要将它从事件等待列表中移除
            // 例如，如果 eventObject != NULL，则需要根据对象类型处理
            break;
        default:
            break;
    }

    // 2. 从 allTaskListHead 中移除
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

    // 3. 如果是删除其他任务，可以直接释放内存
    if (task_to_delete != currentTask) {
        rtos_free(task_to_delete->stack_base);
        rtos_free(task_to_delete);
    } else {
        // 4. 如果是任务自杀，则将其加入待删除列表
        task_to_delete->pNextTask = tasksToDeleteList;
        tasksToDeleteList = task_to_delete;

        // 5. 立即触发调度，切换出去
        MY_RTOS_YIELD();
    }

    MY_RTOS_EXIT_CRITICAL(primask_status);

    // 注意：如果是自杀，代码永远不会执行到这里
    return 0;
}

void Task_Delay(uint32_t tick) {
    if (tick == 0) return;

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    //从就绪列表中移除当前任务
    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);

    currentTask->delay = tick;
    currentTask->state = TASK_STATE_DELAYED;

    //简化: 头插法
    currentTask->pNextReady = delayedTaskListHead;
    currentTask->pPrevReady = NULL;
    if (delayedTaskListHead != NULL) {
        delayedTaskListHead->pPrevReady = currentTask;
    }
    delayedTaskListHead = currentTask;

    MY_RTOS_EXIT_CRITICAL(primask_status);

    MY_RTOS_YIELD();
}

// 任务调度器核心
void *schedule_next_task(void) {
    Task_t *nextTaskToRun = NULL;

    if (topReadyPriority == 0) {
        // 没有就绪任务，只能运行空闲任务
        nextTaskToRun = idleTask;
    } else {
        // AI给的魔法操作: 使用 CLZ (Count Leading Zeros) 指令快速找到最高的置位
        uint32_t highestPriority = 31 - __CLZ(topReadyPriority);

        // 从该优先级的就绪链表头取出任务
        nextTaskToRun = readyTaskLists[highestPriority];

        // 实现同优先级任务的轮询 (Round-Robin)
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
    // 创建空闲任务 优先级最低 (0)
    idleTask = Task_Create(MyRTOS_Idle_Task, 64, NULL, 0);
    if (idleTask == NULL) {
        DBG_PRINTF("Error: Failed to create Idle Task!\n");
        while (1);
    }

    //创建一个队列，用于向定时器服务任务发送命令 长度10差不多
    timerCommandQueue = Queue_Create(10, sizeof(TimerCommand_t));
    if (timerCommandQueue == NULL) {
        DBG_PRINTF("Error: Failed to create Timer Command Queue!\n");
        while (1);
    }
    // MY_RTOS_MAX_PRIORITIES - 1 是最高优先级
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

    // 第一次调度,手动选择下一个任务
    schedule_next_task();

    __asm volatile("svc 0");
    for (;;);
}

int Task_Notify(TaskHandle_t task_h) {
    uint32_t primask_status;
    int trigger_yield = 0; // 是否需要抢占调度的标志

    MY_RTOS_ENTER_CRITICAL(primask_status);

    // 检查任务是否确实在等待通知
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
    // 强制类型转换，因为在.c文件内部  TaskHandle_t 就是 Task_t*
    Task_t *task = task_h;
    if (task == NULL) {
        return TASK_STATE_UNUSED;
    }
    return task->state;
}

uint8_t Task_GetPriority(const TaskHandle_t task_h) {
    Task_t *task = task_h;
    if (task == NULL) {
        // 返回一个无效的优先级
        return (uint8_t) -1;
    }
    return task->priority;
}

TaskHandle_t Task_GetCurrentTaskHandle(void) {
    return currentTask;
}


/*===========================================================================*/
/* 消息队列 (Queue Management)                                    */
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
    pTaskToRemove->pNextEvent = NULL; // 清理指针
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
/* 软件定时器 (Software Timer Management)                         */
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
/* 互斥锁 (Mutex Management)                                      */
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
 * @brief 创建一个互斥锁.
 */
MutexHandle_t Mutex_Create(void) {
    // 1. 为互斥锁结构体动态分配内存
    MutexHandle_t mutex = (MutexHandle_t) rtos_malloc(sizeof(Mutex_t));

    // 2. 检查内存是否分配成功
    if (mutex != NULL) {
        // 3. 调用内部初始化函数来设置初始值
        Mutex_Init(mutex);
    }

    // 4. 返回句柄 (如果分配失败，这里返回的就是 NULL)
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
        // 锁被占用,进入等待
        if (currentTask->taskId < 32) {
            mutex->waiting_mask |= (1 << currentTask->taskId);
        }
        // 从就绪列表移除
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;

        // 触发调度
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);
        // 当任务被唤醒后，会从这里继续执行
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

        // 释放锁
        mutex->locked = 0;
        mutex->owner = (uint32_t) -1;
        mutex->owner_tcb = NULL;

        // 唤醒等待的任务
        if (mutex->waiting_mask != 0) {
            Task_t *p_task = allTaskListHead; // <<< 遍历所有任务
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        // 清除等待标志
                        mutex->waiting_mask &= ~(1 << p_task->taskId);
                        // 添加回就绪列表
                        addTaskToReadyList(p_task);
                        // 如果被唤醒任务的优先级高于当前任务，则需要调度
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
/* 硬件移植层 (Porting Layer - Interrupt Handlers)                */
/*===========================================================================*/

//补充定义
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
            // 如果任务因事件超时，需要将其从事件等待列表中移除
            if (p->eventObject != NULL) {
                Queue_t *pQueue = (Queue_t *) p->eventObject;
                // 这里需要判断是哪个等待列表，简化处理，假设都检查
                removeTaskFromEventList(&pQueue->sendWaitList, p);
                removeTaskFromEventList(&pQueue->receiveWaitList, p);
            }
            addTaskToReadyList(p);
        }
        p = pNext;
    }

    // 每次SysTick都请求调度, 以处理可能发生的抢占
    MY_RTOS_YIELD();

    MY_RTOS_EXIT_CRITICAL(primask_status);
}
__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        " mrs r0, psp                       \n"
        " isb                               \n"

        " ldr r2, =currentTask              \n" /* r2 = &currentTask */
        " ldr r3, [r2]                      \n" /* r3 = currentTask */
        " cbz r3, 1f                        \n" /* 如果 currentTask 为 NULL (首次调度)，则不保存 */

        /* 保存 FPU 上下文 (S16-S31) */
        " tst lr, #0x10                     \n" /* 测试 EXC_RETURN 的 bit 4, 0表示使用了FPU */
        " it eq                             \n" /* <<<<<<< 修复: 恢复了这条关键指令 */
        " vstmdbeq r0!, {s16-s31}           \n" /* 保存 S16-S31 到栈上 */

        /* 保存通用寄存器 (R4-R11) 和 EXC_RETURN */
        " mov r1, lr                        \n" /* 将 EXC_RETURN 值存入 r1 */
        " stmdb r0!, {r1, r4-r11}           \n" /* 保存 EXC_RETURN, R4-R11 */

        " str r0, [r3]                      \n" /* 保存新的栈顶到 currentTask->sp */

        "1:                                 \n"
        " cpsid i                           \n" /* 关中断，保护 schedule_next_task */
        " bl schedule_next_task             \n" /* r0 = nextTask->sp (schedule_next_task的返回值) */
        " cpsie i                           \n" /* 开中断 */

        " ldr r2, =currentTask              \n"
        " ldr r2, [r2]                      \n" /* r2 = currentTask (新的) */
        " ldr r0, [r2]                      \n" /* r0 = currentTask->sp (新的栈顶) */

        /* 恢复通用寄存器 (R4-R11) 和 EXC_RETURN */
        " ldmia r0!, {r1, r4-r11}           \n" /* 恢复 EXC_RETURN 到 r1, R4-R11 */
        " mov lr, r1                        \n" /* 更新 LR 寄存器 */

        /* 恢复 FPU 上下文 */
        " tst lr, #0x10                     \n" /* 再次测试 EXC_RETURN 的 bit 4 */
        " it eq                             \n" /* <<<<<<< 修复: 恢复了这条关键指令 */
        " vldmiaeq r0!, {s16-s31}           \n" /* 恢复 S16-S31 */

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

        /* 恢复通用寄存器 (R4-R11) 和 EXC_RETURN */
        " ldmia r0!, {r1, r4-r11}   \n"
        " mov lr, r1                \n"

        /* 恢复 FPU 上下文 */
        " tst lr, #0x10             \n" // 检查是否需要恢复 FPU 上下文
        " it eq                     \n" // <<<<<<< 修复: 恢复了这条关键指令
        " vldmiaeq r0!, {s16-s31}   \n" // 恢复 S16-S31

        " msr psp, r0               \n" // 恢复 PSP
        " isb                       \n"

        " movs r0, #2               \n" // Thread+PSP
        " msr control, r0           \n"
        " isb                       \n"

        " bx lr                     \n" // 使用恢复的 EXC_RETURN 值返回
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
