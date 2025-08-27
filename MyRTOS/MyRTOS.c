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

typedef struct EventList_t {
    volatile TaskHandle_t head; // 指向等待任务链表的头部
} EventList_t;

// 互斥锁结构体
typedef struct Mutex_t {
    volatile int locked;
    struct Task_t *owner_tcb;
    struct Mutex_t *next_held_mutex;
    EventList_t eventList;
    volatile uint32_t recursion_count;
} Mutex_t;

// 任务控制块 (TCB)
//为了保持汇编代码的兼容性 sp 必须是结构体的第一个成员
typedef struct Task_t {
    uint32_t *sp;

    void (*func)(void *); //任务函数
    void *param; //任务参数
    uint64_t delay; // 延时-绝对系统tick
    volatile uint32_t notification; //保留字段
    volatile uint8_t is_waiting_notification;
    volatile TaskState_t state; //状态
    uint32_t taskId; // ID
    uint32_t *stack_base; // 栈基地址,用于free
    uint8_t priority; // 当前优先级
    uint8_t basePriority; // 基础优先级 (创建时的,不应该变)
    struct Task_t *pNextTask; // 全局任务链表
    struct Task_t *pNextGeneric; // 用于就绪/延时双向链表
    struct Task_t *pPrevGeneric; // 用于就绪/延时双向链表
    struct Task_t *pNextEvent; //用于事件等待单向链表
    EventList_t *pEventList; //指向正在等待的事件列表
    Mutex_t *held_mutexes_head; //持有的互斥锁链表
    void *eventData; //用于队列传递数据指针
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
    uint8_t *storage;
    uint32_t length;
    uint32_t itemSize;
    volatile uint32_t waitingCount;
    uint8_t *writePtr;
    uint8_t *readPtr;
    EventList_t sendEventList;
    EventList_t receiveEventList;
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


//================= Event Management ================
static void eventListInit(EventList_t *pEventList);

static void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert);

static void eventListRemove(TaskHandle_t taskToRemove);

//================= Event Management ================

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

static void task_set_priority(TaskHandle_t task, uint8_t newPriority);

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
        if (nextTaskToRun != NULL && nextTaskToRun->pNextGeneric != NULL) {
            readyTaskLists[highestPriority] = nextTaskToRun->pNextGeneric;
            nextTaskToRun->pNextGeneric->pPrevGeneric = NULL;

            Task_t *pLast = readyTaskLists[highestPriority];
            while (pLast->pNextGeneric != NULL) {
                pLast = pLast->pNextGeneric;
            }
            pLast->pNextGeneric = nextTaskToRun;
            nextTaskToRun->pPrevGeneric = pLast;
            nextTaskToRun->pNextGeneric = NULL;
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


//================= Event Management ================

// 初始化事件列表
static void eventListInit(EventList_t *pEventList) {
    pEventList->head = NULL;
}

// 将任务按优先级插入事件列表
static void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert) {
    taskToInsert->pEventList = pEventList; // 记录任务正在等待的列表

    if (pEventList->head == NULL || pEventList->head->priority <= taskToInsert->priority) {
        // 插入到头部 (列表为空或新任务优先级更高或相等)
        taskToInsert->pNextEvent = pEventList->head;
        pEventList->head = taskToInsert;
    } else {
        // 遍历查找插入点
        Task_t *iterator = pEventList->head;
        while (iterator->pNextEvent != NULL && iterator->pNextEvent->priority > taskToInsert->priority) {
            iterator = iterator->pNextEvent;
        }
        taskToInsert->pNextEvent = iterator->pNextEvent;
        iterator->pNextEvent = taskToInsert;
    }
}

// 从事件列表中移除任务
static void eventListRemove(TaskHandle_t taskToRemove) {
    if (taskToRemove->pEventList == NULL) {
        return; // 任务没有在等待任何事件
    }

    EventList_t *pEventList = taskToRemove->pEventList;

    if (pEventList->head == taskToRemove) {
        pEventList->head = taskToRemove->pNextEvent;
    } else {
        Task_t *iterator = pEventList->head;
        while (iterator != NULL && iterator->pNextEvent != taskToRemove) {
            iterator = iterator->pNextEvent;
        }
        if (iterator != NULL) {
            iterator->pNextEvent = taskToRemove->pNextEvent;
        }
    }

    // 清理任务的指针
    taskToRemove->pNextEvent = NULL;
    taskToRemove->pEventList = NULL;
}

//================= Event Management ================


//================= Task Management ================

// 找到并替换原来的 addTaskToSortedDelayList 函数
static void addTaskToSortedDelayList(TaskHandle_t task) {
    // delay 是唤醒的绝对时间
    const uint64_t wakeUpTime = task->delay;

    if (delayedTaskListHead == NULL || wakeUpTime < delayedTaskListHead->delay) {
        // 插入到头部 (链表为空或新任务比头任务更早唤醒)
        task->pNextGeneric = delayedTaskListHead;
        task->pPrevGeneric = NULL;
        if (delayedTaskListHead != NULL) {
            delayedTaskListHead->pPrevGeneric = task;
        }
        delayedTaskListHead = task;
    } else {
        // 遍历查找插入点， 找到一个 iterator，使得新任务应该被插入在它后面
        Task_t *iterator = delayedTaskListHead;

        // 使用 <= 确保了相同唤醒时间的任务是 FIFO (先进先出) 的
        while (iterator->pNextGeneric != NULL && iterator->pNextGeneric->delay <= wakeUpTime) {
            iterator = iterator->pNextGeneric;
        }

        // 此刻, 新任务应该被插入到 iterator 和 iterator->pNextGeneric 之间

        // 将新任务的 next 指向 iterator 的下一个节点
        task->pNextGeneric = iterator->pNextGeneric;
        if (iterator->pNextGeneric != NULL) {
            iterator->pNextGeneric->pPrevGeneric = task;
        }
        iterator->pNextGeneric = task;
        task->pPrevGeneric = iterator;
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
    task->pNextGeneric = NULL;
    if (readyTaskLists[task->priority] == NULL) {
        // 这是该优先级的第一个任务
        readyTaskLists[task->priority] = task;
        task->pPrevGeneric = NULL;
    } else {
        // 找到链表尾部并插入
        Task_t *pLast = readyTaskLists[task->priority];
        while (pLast->pNextGeneric != NULL) {
            pLast = pLast->pNextGeneric;
        }
        pLast->pNextGeneric = task;
        task->pPrevGeneric = pLast;
    }
    task->state = TASK_STATE_READY;

    MY_RTOS_EXIT_CRITICAL(primask_status);
}

static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t pTaskToRemove) {
    if (pTaskToRemove == NULL) return;

    // 更新前一个节点的 next 指针
    if (pTaskToRemove->pPrevGeneric != NULL) {
        pTaskToRemove->pPrevGeneric->pNextGeneric = pTaskToRemove->pNextGeneric;
    } else {
        // 移除的是头节点
        *ppListHead = pTaskToRemove->pNextGeneric;
    }

    // 更新后一个节点的 prev 指针
    if (pTaskToRemove->pNextGeneric != NULL) {
        pTaskToRemove->pNextGeneric->pPrevGeneric = pTaskToRemove->pPrevGeneric;
    }

    // 清理被移除节点的指针
    pTaskToRemove->pNextGeneric = NULL;
    pTaskToRemove->pPrevGeneric = NULL;

    // 如果任务是从就绪列表中移除，还需要检查是否需要清除优先级位图
    if (pTaskToRemove->state == TASK_STATE_READY) {
        if (readyTaskLists[pTaskToRemove->priority] == NULL) {
            topReadyPriority &= ~(1UL << pTaskToRemove->priority);
        }
    }
}





static void task_set_priority(TaskHandle_t task, uint8_t newPriority) {
    if (task->priority == newPriority) {
        return;
    }
    // 如果任务在就绪列表中，需要将它移动到新的优先级对应的就绪列表
    if (task->state == TASK_STATE_READY) {
        // 从旧的就绪列表移除
        removeTaskFromList(&readyTaskLists[task->priority], task);
        // 更新优先级并添加到新的就绪列表
        task->priority = newPriority;
        addTaskToReadyList(task);
    } else {
        // 如果任务是阻塞或延时的，直接更新优先级即可
        // 当它被唤醒时，会自然地进入正确优先级的就绪列表
        task->priority = newPriority;
    }
}

/**
 * @brief 创建一个新任务
 *
 * @param func 任务函数指针
 * @param stack_size 任务栈大小 (单位: uint32_t)
 * @param param 传递给任务的参数
 * @param priority 任务的初始优先级
 * @return TaskHandle_t 任务句柄，失败则返回 NULL
 */
TaskHandle_t Task_Create(void (*func)(void *), uint16_t stack_size, void *param, uint8_t priority) {
    // 检查优先级是否有效
    if (priority >= MY_RTOS_MAX_PRIORITIES) {
        DBG_PRINTF("Error: Invalid task priority %u.\n", priority);
        return NULL;
    }

    // 为任务控制块 TCB 分配内存
    Task_t *t = rtos_malloc(sizeof(Task_t));
    if (t == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for TCB.\n");
        return NULL;
    }

    // 为任务栈分配内存
    uint32_t stack_size_bytes = stack_size * sizeof(uint32_t);
    uint32_t *stack = rtos_malloc(stack_size_bytes);
    if (stack == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for stack.\n");
        rtos_free(t);
        return NULL;
    }

    // 初始化 TCB 成员
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    t->taskId = nextTaskId++;
    t->stack_base = stack;

    t->priority = priority;
    t->basePriority = priority;

    t->pNextTask = NULL;
    t->pNextGeneric = NULL;
    t->pPrevGeneric = NULL;
    t->pNextEvent = NULL;
    t->pEventList = NULL;
    t->held_mutexes_head = NULL;
    t->eventData = NULL;

    // ----- 栈帧初始化 (与之前保持一致) -----
    uint32_t *sp = stack + stack_size;
    sp = (uint32_t *) (((uintptr_t) sp) & ~0x7u);
    sp--;
    *sp = 0x01000000; // xPSR
    sp--;
    *sp = ((uint32_t) func) | 1u; // PC
    sp--;
    *sp = 0; // LR
    sp--;
    *sp = 0x12121212; // R12
    sp--;
    *sp = 0x03030303; // R3
    sp--;
    *sp = 0x02020202; // R2
    sp--;
    *sp = 0x01010101; // R1
    sp--;
    *sp = (uint32_t) param; // R0
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
    *sp = 0xFFFFFFFD; // EXC_RETURN
    t->sp = sp;
    // ----- 栈帧初始化结束 -----
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    // 添加到所有任务链表
    if (allTaskListHead == NULL) {
        allTaskListHead = t;
    } else {
        Task_t *p = allTaskListHead;
        while (p->pNextTask != NULL) p = p->pNextTask;
        p->pNextTask = t;
    }
    // 添加到就绪列表
    addTaskToReadyList(t);

    MY_RTOS_EXIT_CRITICAL(primask_status);

    DBG_PRINTF("Task %lu created with priority %u. Stack top: %p, Initial SP: %p\n", t->taskId, t->priority,
               &stack[stack_size - 1], t->sp);
    return t;
}

int Task_Delete(TaskHandle_t task_h) {
    Task_t *task_to_delete;
    uint32_t primask_status;
    int trigger_yield = 0; // 是否需要在函数末尾触发调度的标志

    // 确定要删除的目标任务
    if (task_h == NULL) {
        task_to_delete = currentTask; // NULL 表示删除自己
    } else {
        task_to_delete = (Task_t *) task_h;
    }

    // 安全性检查：绝不允许删除空闲任务
    if (task_to_delete == idleTask) {
        return -1;
    }

    // 进入临界区，保护所有内核数据结构
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (task_to_delete->state == TASK_STATE_READY) {
        removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
    } else {
        // 对于非就绪任务，如果它在延时列表里，则移除。
        // `removeTaskFromList` 足够健壮，即使任务不在列表中也不会出错。
        removeTaskFromList(&delayedTaskListHead, task_to_delete);
    }

    //从事件等待列表中移除
    //无需知道任务在等什么，通用接口即可处理。
    if (task_to_delete->pEventList != NULL) {
        eventListRemove(task_to_delete);
    }

    // 自动释放任务持有的所有互斥锁
    while (task_to_delete->held_mutexes_head != NULL) {
        Mutex_t *mutex_to_release = task_to_delete->held_mutexes_head;
        task_to_delete->held_mutexes_head = mutex_to_release->next_held_mutex;
        mutex_to_release->locked = 0;
        mutex_to_release->owner_tcb = NULL;
        // 如果是递归锁 需重置 recursion_count
        if (mutex_to_release->eventList.head != NULL) {
            Task_t *taskToWake = mutex_to_release->eventList.head;
            eventListRemove(taskToWake);
            addTaskToReadyList(taskToWake);
            if (taskToWake->priority > currentTask->priority) {
                trigger_yield = 1;
            }
        }
    }
    task_to_delete->state = TASK_STATE_UNUSED;
    //从全局任务管理链表中移除
    Task_t *prev = NULL;
    Task_t *curr = allTaskListHead;
    while (curr != NULL && curr != task_to_delete) {
        prev = curr;
        curr = curr->pNextTask;
    }

    if (curr != NULL) {
        // 确保任务在列表中
        if (prev == NULL) {
            // 删除的是头节点
            allTaskListHead = curr->pNextTask;
        } else {
            prev->pNextTask = curr->pNextTask;
        }
    }

    if (task_to_delete == currentTask) {
        trigger_yield = 1;
    }
    // 必须在所有链表操作完成后才能释放内存
    rtos_free(task_to_delete->stack_base);
    rtos_free(task_to_delete);

    // 退出临界区
    MY_RTOS_EXIT_CRITICAL(primask_status);

    if (trigger_yield) {
        MY_RTOS_YIELD();
    }
    DBG_PRINTF("Task %d deleted and memory reclaimed.\n", task_to_delete->taskId);
    return 0;
}

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

    eventListInit(&queue->sendEventList);
    eventListInit(&queue->receiveEventList);

    return queue;
}

void Queue_Delete(QueueHandle_t delQueue) {
    Queue_t *queue = delQueue;
    if (queue == NULL) return;

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    // 唤醒所有等待的任务 (它们将从 Send/Receive 调用中失败返回)
    while (queue->sendEventList.head != NULL) {
        Task_t *taskToWake = queue->sendEventList.head;
        eventListRemove(taskToWake);
        addTaskToReadyList(taskToWake);
    }
    while (queue->receiveEventList.head != NULL) {
        Task_t *taskToWake = queue->receiveEventList.head;
        eventListRemove(taskToWake);
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

        // 优先级拦截：检查是否有更高优先级的任务正在等待接收数据
        if (pQueue->receiveEventList.head != NULL) {
            Task_t *taskToWake = pQueue->receiveEventList.head;

            // 将该任务从接收等待列表中移除
            eventListRemove(taskToWake);

            // 如果任务设置了超时，需将其从延时列表中移除
            if (taskToWake->delay > 0) {
                removeTaskFromList(&delayedTaskListHead, taskToWake);
                taskToWake->delay = 0;
            }

            // 直接将数据拷贝到接收任务提供的缓冲区
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            taskToWake->eventData = NULL; // 清理

            addTaskToReadyList(taskToWake);

            if (taskToWake->priority > currentTask->priority) {
                MY_RTOS_YIELD();
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 发送成功
        }

        // 尝试将数据放入队列缓冲区
        if (pQueue->waitingCount < pQueue->length) {
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 发送成功
        }

        // 队列已满，根据 block_ticks 决定是否阻塞
        if (block_ticks == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0; // 不阻塞，直接返回失败
        }

        // 准备阻塞当前任务
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;

        eventListInsert(&pQueue->sendEventList, currentTask);

        currentTask->delay = 0;
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }

        MY_RTOS_EXIT_CRITICAL(primask_status);
        MY_RTOS_YIELD(); // 触发调度

        if (currentTask->pEventList == NULL) {
            // 被 Queue_Receive 正常唤醒，队列现在有空间了，重新循环尝试发送
            continue;
        }
        // 被 SysTick 超时唤醒, pEventList 仍然指向队列的等待列表
        // 需要将自己从等待列表中清理掉
        MY_RTOS_ENTER_CRITICAL(primask_status);
        eventListRemove(currentTask);
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return 0; // 返回超时失败
    }
}

int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) return 0;

    uint32_t primask_status;
    while (1) {
        MY_RTOS_ENTER_CRITICAL(primask_status);

        // 检查队列是否有数据
        if (pQueue->waitingCount > 0) {
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;

            // 检查是否有任务在等待发送（队列空出了位置）
            if (pQueue->sendEventList.head != NULL) {
                Task_t *taskToWake = pQueue->sendEventList.head;
                eventListRemove(taskToWake);

                if (taskToWake->delay > 0) {
                    removeTaskFromList(&delayedTaskListHead, taskToWake);
                    taskToWake->delay = 0;
                }

                addTaskToReadyList(taskToWake);

                if (taskToWake->priority > currentTask->priority) {
                    MY_RTOS_YIELD();
                }
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 接收成功
        }

        // 队列为空，根据 block_ticks 决定是否阻塞
        if (block_ticks == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0;
        }

        // 准备阻塞
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        currentTask->eventData = buffer; // 记录数据要拷贝到的地方

        eventListInsert(&pQueue->receiveEventList, currentTask);

        currentTask->delay = 0;
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }

        MY_RTOS_EXIT_CRITICAL(primask_status);
        MY_RTOS_YIELD();


        if (currentTask->pEventList == NULL) {
            return 1; // 成功接收
        }
        // 被 SysTick 超时唤醒
        MY_RTOS_ENTER_CRITICAL(primask_status);
        eventListRemove(currentTask);
        currentTask->eventData = NULL; // 清理
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
        mutex->owner_tcb = NULL;
        mutex->next_held_mutex = NULL;
        mutex->recursion_count = 0;
        eventListInit(&mutex->eventList); // 初始化事件列表
    }
    return mutex;
}

void Mutex_Lock(MutexHandle_t mutex) {
    uint32_t primask_status;

    while (1) {
        MY_RTOS_ENTER_CRITICAL(primask_status);
        if (!mutex->locked) {
            // 获取锁成功
            mutex->locked = 1;
            mutex->owner_tcb = currentTask;
            // 将锁加入到任务的持有列表
            mutex->next_held_mutex = currentTask->held_mutexes_head;
            currentTask->held_mutexes_head = mutex;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return;
        }
        TaskHandle_t owner_tcb = mutex->owner_tcb;
        if (owner_tcb != NULL && currentTask->priority > owner_tcb->priority) {
            // 当前任务的优先级 > 锁持有者的优先级，触发继承
            // 将锁持有者的优先级提升到和当前任务一样
            task_set_priority(owner_tcb, currentTask->priority);
        }
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&mutex->eventList, currentTask);
        MY_RTOS_EXIT_CRITICAL(primask_status);
        // 必须立即调度，因为锁持有者的优先级可能已被提升
        MY_RTOS_YIELD();
    }
}

void Mutex_Unlock(MutexHandle_t mutex) {
    uint32_t primask_status;
    int trigger_yield = 0;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (!mutex->locked || mutex->owner_tcb != currentTask) {
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return;
    }
    //将锁从当前任务的已持有”链表中移除
    if (currentTask->held_mutexes_head == mutex) {
        //要解锁的锁是链表头
        currentTask->held_mutexes_head = mutex->next_held_mutex;
    } else {
        // 要解锁的锁在链表中间或尾部
        Mutex_t *p_iterator = currentTask->held_mutexes_head;
        while (p_iterator != NULL && p_iterator->next_held_mutex != mutex) {
            p_iterator = p_iterator->next_held_mutex;
        }
        if (p_iterator != NULL) {
            // 找到了前一个节点，进行移除
            p_iterator->next_held_mutex = mutex->next_held_mutex;
        }
    }
    mutex->next_held_mutex = NULL; // 清理指针，好习惯
    // 首先，假设任务将恢复到其基础优先级
    uint8_t new_priority = currentTask->basePriority;
    Mutex_t *p_held_mutex = currentTask->held_mutexes_head;
    while (p_held_mutex != NULL) {
        // 检查这个仍然被持有的锁，是否有任务在等待它
        if (p_held_mutex->eventList.head != NULL) {
            // 如果有，那么当前任务的优先级不能低于等待者中的最高优先级
            if (p_held_mutex->eventList.head->priority > new_priority) {
                new_priority = p_held_mutex->eventList.head->priority;
            }
        }
        p_held_mutex = p_held_mutex->next_held_mutex;
    }
    // 更新当前任务的优先级
    task_set_priority(currentTask, new_priority);

    // 释放锁并唤醒等待者
    mutex->locked = 0;
    mutex->owner_tcb = NULL;

    // 检查是否有任务正在等待这个刚刚被释放的锁
    if (mutex->eventList.head != NULL) {
        // 获取等待队列中的第一个任务 (即最高优先级的任务)
        Task_t *taskToWake = mutex->eventList.head;

        // 将该任务从锁的等待列表中移除
        eventListRemove(taskToWake);

        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) {
            trigger_yield = 1;
        }
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
    if (trigger_yield) {
        MY_RTOS_YIELD();
    }
}


void Mutex_Lock_Recursive(MutexHandle_t mutex) {
    uint32_t primask_status;

    // 检查是否是锁持有者重入
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner_tcb == currentTask) {
        mutex->recursion_count++;
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return; // 直接返回，无需等待
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);

    // 如果不是重入，则执行标准的 Mutex_Lock 逻辑
    Mutex_Lock(mutex);

    // 成功获取锁后，初始化递归计数
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (mutex->owner_tcb == currentTask) {
        // 再次确认
        mutex->recursion_count = 1;
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

void Mutex_Unlock_Recursive(MutexHandle_t mutex) {
    uint32_t primask_status;

    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner_tcb == currentTask) {
        mutex->recursion_count--;
        if (mutex->recursion_count == 0) {
            // 只有当计数器归零时，才真正释放锁
            MY_RTOS_EXIT_CRITICAL(primask_status); // 先退出临界区，让 Unlock 内部自己处理
            Mutex_Unlock(mutex);
        } else {
            MY_RTOS_EXIT_CRITICAL(primask_status);
        }
    } else {
        MY_RTOS_EXIT_CRITICAL(primask_status);
    }
}


//=========== Interrupt Handlers ============

void SysTick_Handler(void) {
    uint32_t primask_status;

    MY_RTOS_ENTER_CRITICAL(primask_status);

    //增加tick
    systemTickCount++;

    const uint64_t current_tick = systemTickCount;

    while (delayedTaskListHead != NULL && delayedTaskListHead->delay <= current_tick) {
        Task_t *taskToWake = delayedTaskListHead;

        delayedTaskListHead = taskToWake->pNextGeneric;
        if (delayedTaskListHead != NULL) {
            delayedTaskListHead->pPrevGeneric = NULL;
        }
        // 清理被移除任务的指针，防止悬挂
        taskToWake->pNextGeneric = NULL;
        taskToWake->pPrevGeneric = NULL;
        taskToWake->delay = 0; // 清除唤醒时间
        addTaskToReadyList(taskToWake);
    }
    MY_RTOS_YIELD();
    // 退出临界区
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
