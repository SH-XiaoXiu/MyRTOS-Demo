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

// 补充定义
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

// 互斥锁结构体
typedef struct Mutex_t {
    volatile int locked;
    volatile uint32_t owner;
    volatile uint32_t waiting_mask;
    struct Task_t *owner_tcb;
    struct Mutex_t *next_held_mutex;
} Mutex_t;

// 任务控制块 (TCB)
//为了保持汇编代码的兼容性 sp 必须是结构体的第一个成员
typedef struct Task_t {
    uint32_t *sp;

    void (*func)(void *); // 任务函数
    void *param; // 任务参数
    uint64_t delay; // 延时
    volatile uint32_t notification;
    volatile uint8_t is_waiting_notification;
    volatile TaskState_t state; // 任务状态
    uint32_t taskId; // 任务ID
    uint32_t *stack_base; // 栈基地址,用于free
    uint8_t priority; //任务优先级
    struct Task_t *pNextTask; //用于所有任务链表
    struct Task_t *pNextReady; //用于就绪或延时链表
    struct Task_t *pPrevReady; //用于双向链表,方便删除 O(1)复杂度
    Mutex_t *held_mutexes_head;
    void *eventObject; // 指向正在等待的内核对象
    void *eventData; // 用于传递与事件相关的数据指针 (如消息的源/目的地址)
    struct Task_t *pNextEvent; // 用于构建内核对象的等待任务链表
} Task_t;

typedef struct Timer_t {
    TimerCallback_t callback; // 定时器到期时执行的回调函数
    void *arg; // 传递给回调函数的额外参数
    uint32_t initialDelay; // 首次触发延时 (in ticks)
    uint32_t period; // 周期 (in ticks), 如果为0则是单次定时器
    volatile uint32_t expiryTime; // 下一次到期时的绝对系统tick
    struct Timer_t *pNext; // 用于构建活动定时器链表
    volatile uint8_t active; // 定时器是否处于活动状态 (0:inactive, 1:active)
} Timer_t;

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

//====================== 动态内存管理 ======================

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
    for (iterator = &start; iterator->nextFreeBlock < blockToInsert;
         iterator = iterator->nextFreeBlock) {
        /* 什么都不用做，只是找到位置 */
    }

    /* 看看新块是否与迭代器指向的块在物理上相邻 */
    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        /* 相邻，合并它们 */
        iterator->blockSize += blockToInsert->blockSize;
        /* 合并后，新块就是迭代器指向的块了 */
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
    // 创建空闲任务 优先级最低 (0)
    idleTask = Task_Create(MyRTOS_Idle_Task, 64,NULL, 0);
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
    SCB->VTOR = (uint32_t) 0x08000000;
    DBG_PRINTF("Starting scheduler...\n");

    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0x00);
    if (SysTick_Config(SystemCoreClock / MY_RTOS_TICK_RATE_HZ)) {
        DBG_PRINTF("Error: SysTick_Config failed\n");
        while (1);
    }

    // 第一次调度,手动选择下一个任务
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

// 任务调度器核心
void *schedule_next_task(void) {
    Task_t *nextTaskToRun = NULL;

    if (topReadyPriority == 0) {
        // 没有就绪任务，只能运行空闲任务
        nextTaskToRun = idleTask;
    } else {
        // AI给的魔法操作: 使用 CLZ (Count Leading Zeros) 指令快速找到最高的置位
        // 31 - __CLZ(topReadyPriority) 能直接得到最高位的索引
        uint32_t highestPriority = 31 - __CLZ(topReadyPriority);

        // 从该优先级的就绪链表头取出任务
        nextTaskToRun = readyTaskLists[highestPriority];

        // 实现同优先级任务的轮询 (Round-Robin)
        // 如果该优先级有多个任务，则将当前选中的任务移到链表尾部
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
        // 不应该哈
        return NULL;
    }

    return currentTask->sp;
}

static void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // 进入低功耗模式，等待中断
    }
}


//================= Task Management ================

// 找到并替换原来的 addTaskToSortedDelayList 函数
static void addTaskToSortedDelayList(TaskHandle_t task) {
    // delay 是唤醒的绝对时间
    const uint64_t wakeUpTime = task->delay;

    if (delayedTaskListHead == NULL || wakeUpTime < delayedTaskListHead->delay) {
        // 插入到头部 (链表为空或新任务比头任务更早唤醒)
        task->pNextReady = delayedTaskListHead;
        task->pPrevReady = NULL;
        if (delayedTaskListHead != NULL) {
            delayedTaskListHead->pPrevReady = task;
        }
        delayedTaskListHead = task;
    } else {
        // 遍历查找插入点， 找到一个 iterator，使得新任务应该被插入在它后面
        Task_t *iterator = delayedTaskListHead;

        // 使用 <= 确保了相同唤醒时间的任务是 FIFO (先进先出) 的
        while (iterator->pNextReady != NULL && iterator->pNextReady->delay <= wakeUpTime) {
            iterator = iterator->pNextReady;
        }

        // 此刻, 新任务应该被插入到 iterator 和 iterator->pNextReady 之间

        // 将新任务的 next 指向 iterator 的下一个节点
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

static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t pTaskToRemove) {
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
        // 插入到头部 (列表为空或新任务优先级更高或相等)
        taskToInsert->pNextEvent = *listHead;
        *listHead = taskToInsert;
    } else {
        // 遍历查找插入点
        Task_t *iterator = *listHead;
        while (iterator->pNextEvent != NULL && iterator->pNextEvent->priority > taskToInsert->priority) {
            iterator = iterator->pNextEvent;
        }
        taskToInsert->pNextEvent = iterator->pNextEvent;
        iterator->pNextEvent = taskToInsert;
    }
}

TaskHandle_t Task_Create(void (*func)(void *), uint16_t stack_size, void *param, uint8_t priority) {
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

    /* 硬件自动保存的栈帧 (R0-R3, R12, LR, PC, xPSR) */
    sp--;
    *sp = 0x01000000; // xPSR (Thumb bit must be 1)
    sp--;
    *sp = ((uint32_t) func) | 1u; // PC (入口点)
    sp--;
    *sp = 0; // LR (任务返回地址，设为0或任务自杀函数)
    sp--;
    *sp = 0x12121212; // R12
    sp--;
    *sp = 0x03030303; // R3
    sp--;
    *sp = 0x02020202; // R2
    sp--;
    *sp = 0x01010101; // R1
    sp--;
    *sp = (uint32_t) param; // R0 (参数)

    /* 软件手动保存的通用寄存器 (R4 - R11) 和 EXC_RETURN */
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
    *sp = 0xFFFFFFFD; // EXC_RETURN: 指示返回时恢复FPU上下文

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
               &stack[stack_size - 1], t->sp);
    return t;
}

int Task_Delete(TaskHandle_t task_h) {
    // 不允许删除 NULL 任务或空闲任务
    if (task_h == idleTask) {
        return -1;
    }
    //删除当前任务
    if (task_h == NULL) {
        task_h = currentTask;
    }
    // 需要修改任务 TCB 的内容，所以需要一个非 const 的指针
    Task_t *task_to_delete = (Task_t *) task_h;
    uint32_t primask_status;
    int trigger_yield = 0; // 是否需要在函数末尾触发调度的标志

    MY_RTOS_ENTER_CRITICAL(primask_status);
    //根据任务当前状态，从对应的状态链表中移除
    if (task_to_delete->state == TASK_STATE_READY) {
        removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
    } else {
        removeTaskFromList(&delayedTaskListHead, task_to_delete);
    }
    //如果任务正在等待内核对象（如队列），则从对象的等待列表中移除。
    if (task_to_delete->state == TASK_STATE_BLOCKED && task_to_delete->eventObject != NULL) {
        // 这里我们只处理了队列，如果有信号量、事件组等，也需要在这里处理
        Queue_t *pQueue = (Queue_t *) task_to_delete->eventObject;
        // 它可能在发送等待列表或接收等待列表
        removeTaskFromEventList(&pQueue->sendWaitList, task_to_delete);
        removeTaskFromEventList(&pQueue->receiveWaitList, task_to_delete);
    }
    // 将任务状态标记为未使用，防止其他地方误操作
    task_to_delete->state = TASK_STATE_UNUSED;
    //自动释放任务持有的所有互斥锁，防止死锁
    Mutex_t *p_mutex = task_to_delete->held_mutexes_head;
    while (p_mutex != NULL) {
        Mutex_t *next_mutex = p_mutex->next_held_mutex;

        // 手动解锁
        p_mutex->locked = 0;
        p_mutex->owner = (uint32_t) -1;
        p_mutex->owner_tcb = NULL;
        p_mutex->next_held_mutex = NULL; // 从持有链中断开

        // 如果有其他任务在等待这个锁，唤醒它们
        if (p_mutex->waiting_mask != 0) {
            Task_t *p_task = allTaskListHead; // 遍历所有任务来查找等待者
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (p_mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        //唤醒任务
                        p_mutex->waiting_mask &= ~(1UL << p_task->taskId); // 清除等待标志
                        addTaskToReadyList(p_task); // 将任务放回就绪列表
                        // 检查是否需要抢占
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
        curr = curr->pNextTask; // <<< 修改点: 使用 pNextTask 指针
    }

    if (curr == NULL) {
        // 任务不在管理链表中，可能已损坏，直接返回
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return -2;
    }

    if (prev == NULL) {
        allTaskListHead = curr->pNextTask;
    } else {
        prev->pNextTask = curr->pNextTask;
    }

    //如果删除的是当前正在运行的任务，必须立即触发调度
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

// 找到并替换原来的 Task_Delay 函数
void Task_Delay(uint32_t tick) {
    if (tick == 0) return;
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    //从就绪列表中移除当前任务
    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
    currentTask->delay = MyRTOS_GetTick() + tick;
    currentTask->state = TASK_STATE_DELAYED;
    //将任务插入到有序的延时链表
    addTaskToSortedDelayList(currentTask);
    MY_RTOS_EXIT_CRITICAL(primask_status);

    MY_RTOS_YIELD();
    __ISB();
}

int Task_Notify(TaskHandle_t task_h) {
    // 查找任务可以在临界区之外进行，提高并发性

    uint32_t primask_status;
    int trigger_yield = 0; // 是否需要抢占调度的标志

    MY_RTOS_ENTER_CRITICAL(primask_status);

    // 检查任务是否确实在等待通知
    if (task_h->is_waiting_notification && task_h->state == TASK_STATE_BLOCKED) {
        // 清除等待标志
        task_h->is_waiting_notification = 0;

        //将任务重新添加到就绪列表中，使其可以被调度
        addTaskToReadyList(task_h);

        //检查是否需要抢占：如果被唤醒的任务优先级高于当前任务
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

    // __ISB 确保流水线被刷新
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


//====================== 消息队列 ======================

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
    // 唤醒所有等待的任务 (它们将从 Send/Receive 调用中失败返回)
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
        return 0; // 检查队列句柄是否有效
    }

    uint32_t primask_status;

    while (1) {
        // 使用 for 循环结构，以便在被唤醒后能重新评估队列状态
        MY_RTOS_ENTER_CRITICAL(primask_status);

        //优先级拦截：检查是否有更高优先级的任务正在等待接收数据
        if (pQueue->receiveWaitList != NULL) {
            // 直接将数据传递给等待的最高优先级任务，不经过队列存储区
            Task_t *taskToWake = pQueue->receiveWaitList;

            // 将该任务从接收等待列表中移除
            removeTaskFromEventList(&pQueue->receiveWaitList, taskToWake);

            // 检查这个被唤醒的任务是否设置了超
            if (taskToWake->delay > 0) {
                // 如果是说明它当初是带超时阻塞的，必须将它从延时列表中也移除
                removeTaskFromList(&delayedTaskListHead, taskToWake);
                taskToWake->delay = 0; // 清零，防止误判
            }
            // 将数据直接拷贝到接收任务提供的缓冲区
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            // 清理任务的事件标志，这是它醒来后判断成功与否的关键
            taskToWake->eventObject = NULL;
            taskToWake->eventData = NULL;
            // 将任务放回就绪列表
            addTaskToReadyList(taskToWake);
            // 如果被唤醒的任务优先级更高，立即进行任务切换
            if (taskToWake->priority > currentTask->priority) {
                MY_RTOS_YIELD();
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 发送成功
        }

        //尝试将数据放入队列缓冲区
        if (pQueue->waitingCount < pQueue->length) {
            // 队列未满，正常存入数据
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            // 处理写指针回绕
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 发送成功
        }

        //队列已满 根据 block_ticks 决定是否阻塞
        if (block_ticks == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0;
        }
        // 准备阻塞当前发送任务

        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        currentTask->eventObject = pQueue;
        // 将当前任务加入到发送等待列表（按优先级排序）
        addTaskToPrioritySortedList(&pQueue->sendWaitList, currentTask);
        currentTask->delay = 0;
        // 如果不是无限期阻塞，则将任务也加入到延时列表
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);

        //任务被唤醒后，将从这里继续执行
        // 检查唤醒原因
        if (currentTask->eventObject == NULL) {
            // 被 Queue_Receive 正常唤醒，意味着队列现在有空间了。
            continue;
        }
        // 被 SysTick 超时唤醒，eventObject 仍然指向队列。
        // 需要将自己从发送等待列表中清理掉。
        MY_RTOS_ENTER_CRITICAL(primask_status);
        removeTaskFromEventList(&pQueue->sendWaitList, currentTask);
        currentTask->eventObject = NULL; // 清理事件对象
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
            //检查队列是否有数据
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;

            if (pQueue->sendWaitList != NULL) {
                Task_t *taskToWake = pQueue->sendWaitList;
                removeTaskFromEventList(&pQueue->sendWaitList, taskToWake);
                // 检查它是否也在延时列表中
                if (taskToWake->delay > 0) {
                    removeTaskFromList(&delayedTaskListHead, taskToWake);
                    taskToWake->delay = 0;
                }
                // 唤醒它，让它自己去重试发送
                taskToWake->eventObject = NULL;
                addTaskToReadyList(taskToWake);

                if (taskToWake->priority > currentTask->priority) {
                    MY_RTOS_YIELD();
                }
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 接收成功
        }
        if (block_ticks == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0;
        }
        //准备阻塞
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        //加入队列的等待列表
        currentTask->eventObject = pQueue;
        currentTask->eventData = buffer;
        addTaskToPrioritySortedList(&pQueue->receiveWaitList, currentTask);
        currentTask->delay = 0;
        //根据 block_ticks 决定是否加入延时列表
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        //触发调度
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);

        // 如果是被 Queue_Send 正常唤醒，它的 eventObject 会被设为 NULL
        if (currentTask->eventObject == NULL) {
            return 1; // 成功接收
        }
        //SysTick 超时唤醒
        MY_RTOS_ENTER_CRITICAL(primask_status);
        removeTaskFromEventList(&pQueue->receiveWaitList, currentTask);
        currentTask->eventObject = NULL; // 清理
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return 0; // 返回超时
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
        timer->active = 0; // 初始为非活动状态
        timer->pNext = NULL;
    }
    return timer;
}

int Timer_Start(const TimerHandle_t timer) {
    return sendCommandToTimerTask(timer, TIMER_CMD_START, 0); // 0表示不阻塞
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
    // 发送命令给队列，通常不阻塞
    if (Queue_Send(timerCommandQueue, &command, block)) {
        return 0; // 发送成功
    }
    return -1; // 队列满，发送失败
}

static void TimerServiceTask(void *pv) {
    TimerCommand_t command;
    uint32_t ticksToWait;
    while (1) {
        //计算需要阻塞等待的时间
        if (activeTimerListHead == NULL) {
            // 没有活动的定时器，无限期等待命令
            ticksToWait = MY_RTOS_MAX_DELAY;
        } else {
            uint64_t nextExpiryTime = activeTimerListHead->expiryTime;
            uint64_t currentTime = MyRTOS_GetTick();
            if (nextExpiryTime <= currentTime) {
                ticksToWait = 0; // 已经过期，立即处理，不等待
            } else {
                ticksToWait = nextExpiryTime - currentTime;
            }
        }
        //阻塞等待命令队列，超时时间为 ticksToWait
        if (Queue_Receive(timerCommandQueue, &command, ticksToWait)) {
            if (command.timer == NULL) continue;
            // A. 收到了新命令，处理命令
            switch (command.command) {
                case TIMER_CMD_START:
                    // 如果定时器已在活动列表，先移除再重新计算
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
                    rtos_free(command.timer); // 释放定时器控制块内存
                    break;
            }
        } else {
            // B. 等待超时，意味着有定时器到期了
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
        // 移除的是头节点
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
    // 清理指针并标记为非活动
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
            // 这种计算方式可以处理任务被长时间阻塞导致错过多个周期的情况。
            // 它会确保下一次唤醒时间总是未来的某个时刻，而不会连续触发。
            uint64_t newExpiryTime = expiredTimer->expiryTime + expiredTimer->period;
            // 如果计算出的新时间仍然在过去，说明系统延迟非常严重，
            // 错过了不止一个周期。我们需要将唤醒时间 追赶
            if (newExpiryTime <= currentTime) {
                // 计算错过了多少个周期
                uint64_t missed_periods = (currentTime - expiredTimer->expiryTime) / expiredTimer->period;
                // 将唤醒时间直接推进到未来
                newExpiryTime = expiredTimer->expiryTime + (missed_periods + 1) * expiredTimer->period;
            }
            expiredTimer->expiryTime = newExpiryTime;
            insertTimerIntoActiveList(expiredTimer);
            expiredTimer->active = 1;
        }
    }
}


//============== 互斥锁 =============
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


//=========== Interrupt Handlers ============

// 找到并替换原来的 SysTick_Handler 函数
void SysTick_Handler(void) {
    uint32_t primask_status;
    systemTickCount++;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    // 获取当前时间戳
    const uint64_t current_tick = systemTickCount;
    // 循环唤醒所有到期的任务
    while (delayedTaskListHead != NULL && delayedTaskListHead->delay <= current_tick) {
        Task_t *taskToWake = delayedTaskListHead;
        //从延时链表中移除 (总是移除头部)
        delayedTaskListHead = taskToWake->pNextReady;
        if (delayedTaskListHead != NULL) {
            delayedTaskListHead->pPrevReady = NULL;
        }
        taskToWake->pNextReady = NULL;
        taskToWake->pPrevReady = NULL;
        //如果是超时阻塞的任务，还需要从事件等待列表移除
        if (taskToWake->state == TASK_STATE_BLOCKED && taskToWake->eventObject != NULL) {
            Queue_t *pQueue = (Queue_t *) taskToWake->eventObject;
            removeTaskFromEventList(&pQueue->sendWaitList, taskToWake);
            removeTaskFromEventList(&pQueue->receiveWaitList, taskToWake);
        }
        //将任务移回就绪列表
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
        " cbz r3, 1f                        \n" /* 如果 currentTask 为 NULL (首次调度)，则不保存 */

        /* 保存 FPU 上下文 (S16-S31) */
        " tst lr, #0x10                     \n" /* 测试 EXC_RETURN 的 bit 4, 0表示使用了FPU */
        " it eq                             \n" /* 如果 bit 4 为0 */
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
        " it eq                             \n"
        " vldmiaeq r0!, {s16-s31}           \n" /* 恢复 S16-S31 */

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

    /* 判断异常返回使用哪条栈 (EXC_RETURN bit2) */
    register uint32_t lr __asm("lr");

    if (lr & 0x4) {
        // 使用 PSP
        sp = __get_PSP();
    } else {
        // 使用 MSP
        sp = __get_MSP();
    }

    stacked_pc = ((uint32_t *) sp)[6]; // PC 存在硬件自动保存帧的第 6 个位置

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

        " ldmia r0!, {r1, r4-r11}   \n" // 恢复 EXC_RETURN 到 r1, 和 R4-R11
        " mov lr, r1                \n" // 将 EXC_RETURN 写入 LR

        " tst lr, #0x10             \n" // 检查是否需要恢复 FPU 上下文
        " it eq                     \n"
        " vldmiaeq r0!, {s16-s31}   \n" // 恢复 S16-S31

        " msr psp, r0               \n" // 恢复 PSP
        " isb                       \n"

        " movs r0, #2               \n" // Thread+PSP
        " msr control, r0           \n"
        " isb                       \n"

        " bx lr                     \n" // 使用恢复的 EXC_RETURN 值返回
    );
}
