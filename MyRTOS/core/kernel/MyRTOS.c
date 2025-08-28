//
// Created by XiaoXiu on 8/22/2025.
//

#include "MyRTOS.h"
#include "MyRTOS_Monitor.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "MyRTOS_Port.h"

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
#include "MyRTOS_Driver_Timer.h"
#endif


//====================== Internal Data Structures & Defines ======================

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
typedef struct Task_t {
    StackType_t *sp;

    void (*func)(void *); //任务函数
    void *param; //任务参数
    uint64_t delay; // 延时-绝对系统tick
    volatile uint32_t notification; //保留字段
    volatile uint8_t is_waiting_notification;
    volatile TaskState_t state; //状态
    uint32_t taskId; // ID
    StackType_t *stack_base; // 栈基地址,用于free
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    uint16_t stackSizeInWords; //运行时统计需要
#endif
    uint8_t priority; // 当前优先级
    uint8_t basePriority; // 基础优先级 (创建时的,不应该变)
    struct Task_t *pNextTask; // 全局任务链表
    struct Task_t *pNextGeneric; // 用于就绪/延时双向链表
    struct Task_t *pPrevGeneric; // 用于就绪/延时双向链表
    struct Task_t *pNextEvent; //用于事件等待单向链表
    EventList_t *pEventList; //指向正在等待的事件列表
    Mutex_t *held_mutexes_head; //持有的互斥锁链表
    void *eventData; //用于队列传递数据指针
#if (MY_RTOS_TASK_NAME_MAX_LEN > 0)
    char taskName[MY_RTOS_TASK_NAME_MAX_LEN];
#endif
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    volatile uint64_t runTimeCounter; //运行时统计需要
#endif
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
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
static size_t minimumEverFreeBytesRemaining = 0U; //历史最小剩余堆
#endif


/*----- System & Task -----*/
static volatile uint8_t systemIsInitialized = 0;
static volatile uint64_t systemTickCount = 0;
TaskHandle_t allTaskListHead = NULL;
TaskHandle_t currentTask = NULL;
static TaskHandle_t idleTask = NULL;
static uint32_t nextTaskId = 0;
static TaskHandle_t readyTaskLists[MY_RTOS_MAX_PRIORITIES];
static TaskHandle_t delayedTaskListHead = NULL;
static volatile uint32_t topReadyPriority = 0;


//================= Event Management ================
static void eventListInit(EventList_t *pEventList);

static void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert);

static void eventListRemove(TaskHandle_t taskToRemove);

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
static void heapInit(void);

static void insertBlockIntoFreeList(BlockLink_t *blockToInsert);

static void *rtos_malloc(size_t wantedSize);

static void rtos_free(void *pv);

void *schedule_next_task(void);

static void MyRTOS_Idle_Task(void *pv);

static void addTaskToSortedDelayList(TaskHandle_t task);

static void addTaskToReadyList(TaskHandle_t task);

static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t pTaskToRemove);

static void task_set_priority(TaskHandle_t task, uint8_t newPriority);

static void TimerServiceTask(void *pv);

static void insertTimerIntoActiveList(Timer_t *timerToInsert);

static void removeTimerFromActiveList(Timer_t *timerToRemove);

static void processExpiredTimers(void);

static int sendCommandToTimerTask(TimerHandle_t timer, TimerCommandType_t cmd, int block);


//====================== Function Implementations ======================

//====================== 动态内存管理 ======================
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
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    minimumEverFreeBytesRemaining = freeBytesRemaining;
#endif
    blockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * 8) - 1);
}

static void insertBlockIntoFreeList(BlockLink_t *blockToInsert) {
    BlockLink_t *iterator;
    uint8_t *puc;

    for (iterator = &start; iterator->nextFreeBlock < blockToInsert;
         iterator = iterator->nextFreeBlock) {
    }

    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        iterator->blockSize += blockToInsert->blockSize;
        blockToInsert = iterator;
    } else {
        blockToInsert->nextFreeBlock = iterator->nextFreeBlock;
    }

    puc = (uint8_t *) blockToInsert;
    if ((puc + blockToInsert->blockSize) == (uint8_t *) iterator->nextFreeBlock) {
        if (iterator->nextFreeBlock != blockLinkEnd) {
            blockToInsert->blockSize += iterator->nextFreeBlock->blockSize;
            blockToInsert->nextFreeBlock = iterator->nextFreeBlock->nextFreeBlock;
        }
    }

    if (iterator != blockToInsert) {
        iterator->nextFreeBlock = blockToInsert;
    }
}

static void *rtos_malloc(const size_t wantedSize) {
    BlockLink_t *block, *previousBlock, *newBlockLink;
    void *pvReturn = NULL;
    uint32_t primask_status;

    MyRTOS_Port_ENTER_CRITICAL(primask_status); {
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
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
                    if (freeBytesRemaining < minimumEverFreeBytesRemaining) {
                        minimumEverFreeBytesRemaining = freeBytesRemaining;
                    }
#endif
                    block->blockSize |= blockAllocatedBit;
                    block->nextFreeBlock = NULL;
                }
            }
        }
    }
    MyRTOS_Port_EXIT_CRITICAL(primask_status);

    return pvReturn;
}

static void rtos_free(void *pv) {
    if (pv == NULL) return;

    uint8_t *puc = (uint8_t *) pv;
    BlockLink_t *link;
    uint32_t primask_status;

    puc -= heapStructSize;
    link = (BlockLink_t *) puc;

    if (((link->blockSize & blockAllocatedBit) != 0) && (link->nextFreeBlock == NULL)) {
        link->blockSize &= ~blockAllocatedBit;
        MyRTOS_Port_ENTER_CRITICAL(primask_status); {
            freeBytesRemaining += link->blockSize;
            insertBlockIntoFreeList(link);
        }
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
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
    systemIsInitialized = 1;
    MY_RTOS_KERNEL_LOGD("MyRTOS Initialized..\n");
}

uint64_t MyRTOS_GetTick(void) {
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    const uint64_t tick_value = systemTickCount;
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
    return tick_value;
}

void Task_StartScheduler(void) {
    idleTask = Task_Create(MyRTOS_Idle_Task, "IDLE", 64, NULL, 0);
    if (idleTask == NULL) {
        MY_RTOS_KERNEL_LOGE("Failed to create Idle Task!.\n");
        while (1);
    }

    timerCommandQueue = Queue_Create(10, sizeof(TimerCommand_t));
    if (timerCommandQueue == NULL) {
        MY_RTOS_KERNEL_LOGE("Failed to create Timer Command Queue!.\n");
        while (1);
    }
    timerServiceTaskHandle = Task_Create(TimerServiceTask, "TIM", 256, NULL, MY_RTOS_MAX_PRIORITIES - 1);
    if (timerServiceTaskHandle == NULL) {
        MY_RTOS_KERNEL_LOGE("Failed to create Timer Service Task!.\n");
        while (1);
    }

    MY_RTOS_KERNEL_LOGD("Starting scheduler....\n");

    if (MyRTOS_Port_StartScheduler() != 0) {
        // Should not get here
    }

    for (;;);
}

void *schedule_next_task(void) {
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    static TimerHandle_dev_t stats_timer_handle = NULL;
    static uint32_t lastPerfCounter = 0;
    uint32_t currentPerfCounter;

    if (stats_timer_handle == NULL) {
        stats_timer_handle = MyRTOS_Timer_GetHandle(MY_RTOS_STATS_TIMER_ID);
    }

    if (stats_timer_handle != NULL) {
        currentPerfCounter = MyRTOS_Timer_GetCount(stats_timer_handle);
        if (currentTask != NULL) {
            uint32_t timeElapsed;
            if (currentPerfCounter >= lastPerfCounter) {
                timeElapsed = currentPerfCounter - lastPerfCounter;
            } else {
                // Handle counter wrap around
                timeElapsed = (0xFFFFFFFF - lastPerfCounter) + currentPerfCounter + 1;
            }
            currentTask->runTimeCounter += timeElapsed;
        }
        lastPerfCounter = currentPerfCounter;
    }
#endif

    Task_t *nextTaskToRun = NULL;

    if (topReadyPriority == 0) {
        nextTaskToRun = idleTask;
    } else {
        uint32_t highestPriority = 31 - __builtin_clz(topReadyPriority);
        nextTaskToRun = readyTaskLists[highestPriority];

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
        return NULL;
    }

    return currentTask->sp;
}

static void MyRTOS_Idle_Task(void *pv) {
    MY_RTOS_KERNEL_LOGD("Idle Task starting..\n");
    while (1) {
        __asm volatile ("wfi");
    }
}


//================= Event Management ================
static void eventListInit(EventList_t *pEventList) {
    pEventList->head = NULL;
}

static void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert) {
    taskToInsert->pEventList = pEventList;
    if (pEventList->head == NULL || pEventList->head->priority <= taskToInsert->priority) {
        taskToInsert->pNextEvent = pEventList->head;
        pEventList->head = taskToInsert;
    } else {
        Task_t *iterator = pEventList->head;
        while (iterator->pNextEvent != NULL && iterator->pNextEvent->priority > taskToInsert->priority) {
            iterator = iterator->pNextEvent;
        }
        taskToInsert->pNextEvent = iterator->pNextEvent;
        iterator->pNextEvent = taskToInsert;
    }
}

static void eventListRemove(TaskHandle_t taskToRemove) {
    if (taskToRemove->pEventList == NULL) {
        return;
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
    taskToRemove->pNextEvent = NULL;
    taskToRemove->pEventList = NULL;
}

//================= Task Management ================
static void addTaskToSortedDelayList(TaskHandle_t task) {
    const uint64_t wakeUpTime = task->delay;
    if (delayedTaskListHead == NULL || wakeUpTime < delayedTaskListHead->delay) {
        task->pNextGeneric = delayedTaskListHead;
        task->pPrevGeneric = NULL;
        if (delayedTaskListHead != NULL) {
            delayedTaskListHead->pPrevGeneric = task;
        }
        delayedTaskListHead = task;
    } else {
        Task_t *iterator = delayedTaskListHead;
        while (iterator->pNextGeneric != NULL && iterator->pNextGeneric->delay <= wakeUpTime) {
            iterator = iterator->pNextGeneric;
        }
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
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    topReadyPriority |= (1UL << task->priority);
    task->pNextGeneric = NULL;
    if (readyTaskLists[task->priority] == NULL) {
        readyTaskLists[task->priority] = task;
        task->pPrevGeneric = NULL;
    } else {
        Task_t *pLast = readyTaskLists[task->priority];
        while (pLast->pNextGeneric != NULL) {
            pLast = pLast->pNextGeneric;
        }
        pLast->pNextGeneric = task;
        task->pPrevGeneric = pLast;
    }
    task->state = TASK_STATE_READY;
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
}

static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t pTaskToRemove) {
    if (pTaskToRemove == NULL) return;
    if (pTaskToRemove->pPrevGeneric != NULL) {
        pTaskToRemove->pPrevGeneric->pNextGeneric = pTaskToRemove->pNextGeneric;
    } else {
        *ppListHead = pTaskToRemove->pNextGeneric;
    }
    if (pTaskToRemove->pNextGeneric != NULL) {
        pTaskToRemove->pNextGeneric->pPrevGeneric = pTaskToRemove->pPrevGeneric;
    }
    pTaskToRemove->pNextGeneric = NULL;
    pTaskToRemove->pPrevGeneric = NULL;
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
    if (task->state == TASK_STATE_READY) {
        removeTaskFromList(&readyTaskLists[task->priority], task);
        task->priority = newPriority;
        addTaskToReadyList(task);
    } else {
        task->priority = newPriority;
    }
}

TaskHandle_t Task_Create(void (*func)(void *),
                         const char *taskName,
                         uint16_t stack_size,
                         void *param,
                         uint8_t priority) {
    if (priority >= MY_RTOS_MAX_PRIORITIES) {
        MY_RTOS_KERNEL_LOGE("Invalid task priority %u.", priority);
        return NULL;
    }

    Task_t *t = rtos_malloc(sizeof(Task_t));
    if (t == NULL) {
        MY_RTOS_KERNEL_LOGE("Failed to allocate memory for TCB.\n");
        return NULL;
    }

    StackType_t *stack = rtos_malloc(stack_size * sizeof(StackType_t));
    if (stack == NULL) {
        MY_RTOS_KERNEL_LOGE("Failed to allocate memory for stack.\n");
        rtos_free(t);
        return NULL;
    }

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    memset(stack, 0xA5, stack_size * sizeof(StackType_t));
#endif

    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    t->taskId = nextTaskId++;
#if (MY_RTOS_TASK_NAME_MAX_LEN > 0)
    if (taskName != NULL) {
        strncpy(t->taskName, taskName, MY_RTOS_TASK_NAME_MAX_LEN - 1);
        t->taskName[MY_RTOS_TASK_NAME_MAX_LEN - 1] = '\0';
    } else {
        snprintf(t->taskName, MY_RTOS_TASK_NAME_MAX_LEN, "Task%lu", t->taskId);
    }
#endif
    t->stack_base = stack;
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    t->stackSizeInWords = stack_size;
    t->runTimeCounter = 0;
#endif
    t->priority = priority;
    t->basePriority = priority;
    t->pNextTask = NULL;
    t->pNextGeneric = NULL;
    t->pPrevGeneric = NULL;
    t->pNextEvent = NULL;
    t->pEventList = NULL;
    t->held_mutexes_head = NULL;
    t->eventData = NULL;

    t->sp = MyRTOS_Port_InitialiseStack(stack + stack_size, func, param);

    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    if (allTaskListHead == NULL) {
        allTaskListHead = t;
    } else {
        Task_t *p = allTaskListHead;
        while (p->pNextTask != NULL) p = p->pNextTask;
        p->pNextTask = t;
    }
    addTaskToReadyList(t);
    MyRTOS_Port_EXIT_CRITICAL(primask_status);

    MY_RTOS_KERNEL_LOGD("Task '%s' created with priority %u.", taskName, t->priority);
    return t;
}

int Task_Delete(TaskHandle_t task_h) {
    Task_t *task_to_delete;
    uint32_t primask_status;
    int trigger_yield = 0;

    if (task_h == NULL) {
        task_to_delete = currentTask;
    } else {
        task_to_delete = (Task_t *) task_h;
    }

    if (task_to_delete == idleTask) {
        return -1;
    }

    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    if (task_to_delete->state == TASK_STATE_READY) {
        removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
    } else {
        removeTaskFromList(&delayedTaskListHead, task_to_delete);
    }

    if (task_to_delete->pEventList != NULL) {
        eventListRemove(task_to_delete);
    }

    while (task_to_delete->held_mutexes_head != NULL) {
        Mutex_t *mutex_to_release = task_to_delete->held_mutexes_head;
        task_to_delete->held_mutexes_head = mutex_to_release->next_held_mutex;
        mutex_to_release->locked = 0;
        mutex_to_release->owner_tcb = NULL;
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
    Task_t *prev = NULL;
    Task_t *curr = allTaskListHead;
    while (curr != NULL && curr != task_to_delete) {
        prev = curr;
        curr = curr->pNextTask;
    }

    if (curr != NULL) {
        if (prev == NULL) {
            allTaskListHead = curr->pNextTask;
        } else {
            prev->pNextTask = curr->pNextTask;
        }
    }

    if (task_to_delete == currentTask) {
        trigger_yield = 1;
    }

    uint32_t deleted_task_id = task_to_delete->taskId;

    rtos_free(task_to_delete->stack_base);
    rtos_free(task_to_delete);

    MyRTOS_Port_EXIT_CRITICAL(primask_status);

    if (trigger_yield) {
        MyRTOS_Port_YIELD();
    }
    MY_RTOS_KERNEL_LOGD("Task %lu deleted.", deleted_task_id);
    return 0;
}

void Task_Delay(uint32_t tick) {
    if (tick == 0) return;
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
    currentTask->delay = MyRTOS_GetTick() + tick;
    currentTask->state = TASK_STATE_DELAYED;
    addTaskToSortedDelayList(currentTask);
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
    MyRTOS_Port_YIELD();
}

int Task_Notify(TaskHandle_t task_h) {
    uint32_t primask_status;
    int trigger_yield = 0;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    if (task_h->is_waiting_notification && task_h->state == TASK_STATE_BLOCKED) {
        task_h->is_waiting_notification = 0;
        addTaskToReadyList(task_h);
        if (task_h->priority > currentTask->priority) {
            trigger_yield = 1;
        }
    }
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
    if (trigger_yield) {
        MyRTOS_Port_YIELD();
    }
    return 0;
}

void Task_Wait(void) {
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
    currentTask->is_waiting_notification = 1;
    currentTask->state = TASK_STATE_BLOCKED;
    MyRTOS_Port_YIELD();
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
}

TaskState_t Task_GetState(TaskHandle_t task_h) {
    return task_h ? task_h->state : TASK_STATE_UNUSED;
}

uint8_t Task_GetPriority(TaskHandle_t task_h) {
    return task_h ? task_h->priority : 0;
}

TaskHandle_t Task_GetCurrentTaskHandle(void) {
    return currentTask;
}


//====================== 消息队列 ======================
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
    eventListInit(&queue->sendEventList);
    eventListInit(&queue->receiveEventList);
    return queue;
}

void Queue_Delete(QueueHandle_t delQueue) {
    Queue_t *queue = delQueue;
    if (queue == NULL) return;
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
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
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
}

int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) return 0;
    uint32_t primask_status;
    while (1) {
        MyRTOS_Port_ENTER_CRITICAL(primask_status);
        if (pQueue->receiveEventList.head != NULL) {
            Task_t *taskToWake = pQueue->receiveEventList.head;
            eventListRemove(taskToWake);
            if (taskToWake->delay > 0) {
                removeTaskFromList(&delayedTaskListHead, taskToWake);
                taskToWake->delay = 0;
            }
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            taskToWake->eventData = NULL;
            addTaskToReadyList(taskToWake);
            if (taskToWake->priority > currentTask->priority) {
                MyRTOS_Port_YIELD();
            }
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            return 1;
        }
        if (pQueue->waitingCount < pQueue->length) {
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            return 1;
        }
        if (block_ticks == 0) {
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            return 0;
        }
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&pQueue->sendEventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
        MyRTOS_Port_YIELD();
        if (currentTask->pEventList == NULL) {
            continue;
        }
        MyRTOS_Port_ENTER_CRITICAL(primask_status);
        eventListRemove(currentTask);
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
        return 0;
    }
}

int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) return 0;
    uint32_t primask_status;
    while (1) {
        MyRTOS_Port_ENTER_CRITICAL(primask_status);
        if (pQueue->waitingCount > 0) {
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;
            if (pQueue->sendEventList.head != NULL) {
                Task_t *taskToWake = pQueue->sendEventList.head;
                eventListRemove(taskToWake);
                if (taskToWake->delay > 0) {
                    removeTaskFromList(&delayedTaskListHead, taskToWake);
                    taskToWake->delay = 0;
                }
                addTaskToReadyList(taskToWake);
                if (taskToWake->priority > currentTask->priority) {
                    MyRTOS_Port_YIELD();
                }
            }
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            return 1;
        }
        if (block_ticks == 0) {
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            return 0;
        }
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        currentTask->eventData = buffer;
        eventListInsert(&pQueue->receiveEventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
        MyRTOS_Port_YIELD();
        if (currentTask->pEventList == NULL) {
            return 1;
        }
        MyRTOS_Port_ENTER_CRITICAL(primask_status);
        eventListRemove(currentTask);
        currentTask->eventData = NULL;
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
        return 0;
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
        timer->active = 0;
        timer->pNext = NULL;
    }
    return timer;
}

int Timer_Start(const TimerHandle_t timer) {
    return sendCommandToTimerTask(timer, TIMER_CMD_START, 0);
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
    if (Queue_Send(timerCommandQueue, &command, block)) {
        return 0;
    }
    return -1;
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
            if (nextExpiryTime <= currentTime) {
                ticksToWait = 0;
            } else {
                ticksToWait = nextExpiryTime - currentTime;
            }
        }
        if (Queue_Receive(timerCommandQueue, &command, ticksToWait)) {
            if (command.timer == NULL) continue;
            switch (command.command) {
                case TIMER_CMD_START:
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
                    rtos_free(command.timer);
                    break;
            }
        } else {
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
            uint64_t newExpiryTime = expiredTimer->expiryTime + expiredTimer->period;
            if (newExpiryTime <= currentTime) {
                uint64_t missed_periods = (currentTime - expiredTimer->expiryTime) / expiredTimer->period;
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
        eventListInit(&mutex->eventList);
    }
    return mutex;
}

int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks) {
    uint32_t primask_status;
    while (1) {
        MyRTOS_Port_ENTER_CRITICAL(primask_status);
        if (!mutex->locked) {
            mutex->locked = 1;
            mutex->owner_tcb = currentTask;
            mutex->next_held_mutex = currentTask->held_mutexes_head;
            currentTask->held_mutexes_head = mutex;
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            return 1;
        }
        if (block_ticks == 0) {
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            return 0;
        }
        TaskHandle_t owner_tcb = mutex->owner_tcb;
        if (owner_tcb != NULL && currentTask->priority > owner_tcb->priority) {
            task_set_priority(owner_tcb, currentTask->priority);
        }
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&mutex->eventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MY_RTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
        MyRTOS_Port_YIELD();
        if (mutex->owner_tcb == currentTask) {
            return 1;
        }
        if (currentTask->pEventList != NULL) {
            MyRTOS_Port_ENTER_CRITICAL(primask_status);
            eventListRemove(currentTask);
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            return 0;
        }
    }
}

void Mutex_Lock(MutexHandle_t mutex) {
    Mutex_Lock_Timeout(mutex, MY_RTOS_MAX_DELAY);
}

void Mutex_Unlock(MutexHandle_t mutex) {
    uint32_t primask_status;
    int trigger_yield = 0;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    if (!mutex->locked || mutex->owner_tcb != currentTask) {
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
        return;
    }
    if (currentTask->held_mutexes_head == mutex) {
        currentTask->held_mutexes_head = mutex->next_held_mutex;
    } else {
        Mutex_t *p_iterator = currentTask->held_mutexes_head;
        while (p_iterator != NULL && p_iterator->next_held_mutex != mutex) {
            p_iterator = p_iterator->next_held_mutex;
        }
        if (p_iterator != NULL) {
            p_iterator->next_held_mutex = mutex->next_held_mutex;
        }
    }
    mutex->next_held_mutex = NULL;
    uint8_t new_priority = currentTask->basePriority;
    Mutex_t *p_held_mutex = currentTask->held_mutexes_head;
    while (p_held_mutex != NULL) {
        if (p_held_mutex->eventList.head != NULL) {
            if (p_held_mutex->eventList.head->priority > new_priority) {
                new_priority = p_held_mutex->eventList.head->priority;
            }
        }
        p_held_mutex = p_held_mutex->next_held_mutex;
    }
    task_set_priority(currentTask, new_priority);
    mutex->locked = 0;
    mutex->owner_tcb = NULL;
    if (mutex->eventList.head != NULL) {
        Task_t *taskToWake = mutex->eventList.head;
        eventListRemove(taskToWake);
        mutex->locked = 1;
        mutex->owner_tcb = taskToWake;
        taskToWake->held_mutexes_head = mutex;
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) {
            trigger_yield = 1;
        }
    }
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
    if (trigger_yield) {
        MyRTOS_Port_YIELD();
    }
}

void Mutex_Lock_Recursive(MutexHandle_t mutex) {
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner_tcb == currentTask) {
        mutex->recursion_count++;
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
        return;
    }
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
    Mutex_Lock(mutex);
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    if (mutex->owner_tcb == currentTask) {
        mutex->recursion_count = 1;
    }
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
}

void Mutex_Unlock_Recursive(MutexHandle_t mutex) {
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner_tcb == currentTask) {
        mutex->recursion_count--;
        if (mutex->recursion_count == 0) {
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
            Mutex_Unlock(mutex);
        } else {
            MyRTOS_Port_EXIT_CRITICAL(primask_status);
        }
    } else {
        MyRTOS_Port_EXIT_CRITICAL(primask_status);
    }
}


//=========== Tick Handler (called by port) ============
void MyRTOS_Tick_Handler(void) {
    systemTickCount++;
    const uint64_t current_tick = systemTickCount;
    while (delayedTaskListHead != NULL && delayedTaskListHead->delay <= current_tick) {
        Task_t *taskToWake = delayedTaskListHead;
        delayedTaskListHead = taskToWake->pNextGeneric;
        if (delayedTaskListHead != NULL) {
            delayedTaskListHead->pPrevGeneric = NULL;
        }
        taskToWake->pNextGeneric = NULL;
        taskToWake->pPrevGeneric = NULL;
        taskToWake->delay = 0;
        if (taskToWake->pEventList != NULL) {
            eventListRemove(taskToWake);
        }
        addTaskToReadyList(taskToWake);
    }
}


// ==================运行时统计==================
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
static uint32_t prvCalculateStackHighWaterMark(TaskHandle_t task) {
    if (task == NULL || task->stack_base == NULL) {
        return 0;
    }
    const uint32_t *stack_ptr = task->stack_base;
    uint32_t free_words = 0;
    while ((stack_ptr < (task->stack_base + task->stackSizeInWords)) && (*stack_ptr == 0xA5A5A5A5)) {
        stack_ptr++;
        free_words++;
    }
    return (task->stackSizeInWords - free_words) * sizeof(uint32_t);
}

void Task_GetInfo(TaskHandle_t taskHandle, TaskStats_t *pTaskStats) {
    if (taskHandle == NULL || pTaskStats == NULL) return;
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    pTaskStats->taskId = taskHandle->taskId;
#if (MY_RTOS_TASK_NAME_MAX_LEN > 0)
    strncpy(pTaskStats->taskName, taskHandle->taskName, MY_RTOS_TASK_NAME_MAX_LEN);
#endif
    pTaskStats->state = taskHandle->state;
    pTaskStats->currentPriority = taskHandle->priority;
    pTaskStats->basePriority = taskHandle->basePriority;
    pTaskStats->runTimeCounter = taskHandle->runTimeCounter;
    pTaskStats->stackSize = taskHandle->stackSizeInWords * sizeof(uint32_t);
    pTaskStats->stackHighWaterMark = prvCalculateStackHighWaterMark(taskHandle);
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
}

TaskHandle_t Task_GetNextTaskHandle(TaskHandle_t lastTaskHandle) {
    uint32_t primask_status;
    TaskHandle_t nextTask = NULL;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    if (lastTaskHandle == NULL) {
        nextTask = allTaskListHead;
    } else {
        nextTask = lastTaskHandle->pNextTask;
    }
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
    return nextTask;
}

void Heap_GetStats(HeapStats_t *pHeapStats) {
    if (pHeapStats == NULL) return;
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);
    pHeapStats->totalHeapSize = RTOS_MEMORY_POOL_SIZE;
    pHeapStats->freeBytesRemaining = freeBytesRemaining;
    pHeapStats->minimumEverFreeBytesRemaining = minimumEverFreeBytesRemaining;
    MyRTOS_Port_EXIT_CRITICAL(primask_status);
}

#endif // MY_RTOS_GENERATE_RUN_TIME_STATS
