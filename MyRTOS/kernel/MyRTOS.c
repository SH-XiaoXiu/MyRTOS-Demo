#include "MyRTOS.h"
#include "MyRTOS_Port.h"
#include <string.h>
#include "MyRTOS_Extension.h"
#include "MyRTOS_Kernel_Private.h"

/*===========================================================================*
* 私有函数声明
*===========================================================================*/

// 初始化内存堆
static void heapInit(void);

// 将内存块插入到空闲链表中
static void insertBlockIntoFreeList(BlockLink_t *blockToInsert);

// RTOS内部使用的内存分配函数
static void *rtos_malloc(const size_t wantedSize);

// RTOS内部使用的内存释放函数
static void rtos_free(void *pv);

// 将任务添加到就绪链表
static void addTaskToReadyList(TaskHandle_t task);

// 从通用链表中移除任务
static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t taskToRemove);

// 设置任务的优先级
static void task_set_priority(TaskHandle_t task, uint8_t newPriority);

// 将任务添加到按唤醒时间排序的延迟链表中
static void addTaskToSortedDelayList(TaskHandle_t task);

// 初始化事件列表
static void eventListInit(EventList_t *pEventList);

// 将任务插入到事件列表（按优先级排序）
static void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert);

// 从其所属的事件列表中移除任务
static void eventListRemove(TaskHandle_t taskToRemove);

// 广播内核事件给所有已注册的扩展
static void broadcast_event(const KernelEventData_t *pEventData);

// 内存堆中允许的最小内存块大小，至少能容纳两个BlockLink_t结构体
#define HEAP_MINIMUM_BLOCK_SIZE ((sizeof(BlockLink_t) * 2))

// 内存块管理结构的大小，已考虑内存对齐
static const size_t heapStructSize = (sizeof(BlockLink_t) + (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) & ~((
                                         (size_t) MYRTOS_HEAP_BYTE_ALIGNMENT - 1));

//内存堆管理
// 静态分配的内存池，用于RTOS的动态内存分配
static uint8_t rtos_memory_pool[MYRTOS_MEMORY_POOL_SIZE] __attribute__((aligned(MYRTOS_HEAP_BYTE_ALIGNMENT)));
// 空闲内存块链表的起始哨兵节点
static BlockLink_t start;
// 内存堆的结束哨兵节点
static BlockLink_t *blockLinkEnd = NULL;
// 当前剩余的空闲字节数
size_t freeBytesRemaining = 0U;
// 用于标记内存块是否已被分配的位掩码 (最高位)
static size_t blockAllocatedBit = 0;

//内核状态
// 调度器是否已启动的标志
volatile uint8_t g_scheduler_started = 0;
// 临界区嵌套计数
volatile uint32_t criticalNestingCount = 0;
// 系统滴答计数器
static volatile uint64_t systemTickCount = 0;
// 所有已创建任务的链表头
TaskHandle_t allTaskListHead = NULL;
// 当前正在运行的任务的句柄
TaskHandle_t currentTask = NULL;
// 空闲任务的句柄
TaskHandle_t idleTask = NULL;
// 用于分配下一个任务ID的计数器
static uint32_t nextTaskId = 0;
// 任务ID的位图，用于快速查找可用的ID
static uint64_t taskIdBitmap = 0;
// 按优先级组织的就绪任务链表数组
static TaskHandle_t readyTaskLists[MYRTOS_MAX_PRIORITIES];
// 延迟任务链表的头
static TaskHandle_t delayedTaskListHead = NULL;
// 一个位图，用于快速查找当前存在的最高优先级的就绪任务
static volatile uint32_t topReadyPriority = 0;
// 内核扩展回调函数数组
static KernelExtensionCallback_t g_extensions[MAX_KERNEL_EXTENSIONS] = {NULL};
// 已注册的内核扩展数量
static uint8_t g_extension_count = 0;

/**
 * @brief 初始化内存堆
 * @note  此函数负责设置内存池，创建初始的空闲内存块，并设置起始和结束哨兵节点。
 */
static void heapInit(void) {
    BlockLink_t *firstFreeBlock;
    uint8_t *alignedHeap;
    size_t address = (size_t) rtos_memory_pool;
    size_t totalHeapSize = MYRTOS_MEMORY_POOL_SIZE;
    // 确保堆的起始地址是对齐的
    if ((address & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) != 0) {
        address += (MYRTOS_HEAP_BYTE_ALIGNMENT - (address & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)));
        totalHeapSize -= address - (size_t) rtos_memory_pool;
    }
    alignedHeap = (uint8_t *) address;
    // 设置起始哨兵节点
    start.nextFreeBlock = (BlockLink_t *) alignedHeap;
    start.blockSize = (size_t) 0;
    // 设置结束哨兵节点
    address = ((size_t) alignedHeap) + totalHeapSize - heapStructSize;
    blockLinkEnd = (BlockLink_t *) address;
    blockLinkEnd->blockSize = 0;
    blockLinkEnd->nextFreeBlock = NULL;
    // 创建第一个大的空闲内存块
    firstFreeBlock = (BlockLink_t *) alignedHeap;
    firstFreeBlock->blockSize = address - (size_t) firstFreeBlock;
    firstFreeBlock->nextFreeBlock = blockLinkEnd;
    // 初始化剩余空闲字节数
    freeBytesRemaining = firstFreeBlock->blockSize;
    // 设置用于标记“已分配”的位（最高位）
    blockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * 8) - 1);
}

/**
 * @brief 将一个内存块插入到空闲链表中
 * @note  此函数会按地址顺序插入内存块，并尝试与相邻的空闲块合并。
 * @param blockToInsert 要插入的内存块指针
 */
static void insertBlockIntoFreeList(BlockLink_t *blockToInsert) {
    BlockLink_t *iterator;
    uint8_t *puc;
    // 遍历空闲链表，找到合适的插入位置
    for (iterator = &start; iterator->nextFreeBlock < blockToInsert; iterator = iterator->nextFreeBlock) {
        // 空循环，仅为移动迭代器
    }
    // 尝试与前一个空闲块合并
    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        iterator->blockSize += blockToInsert->blockSize;
        blockToInsert = iterator;
    } else {
        blockToInsert->nextFreeBlock = iterator->nextFreeBlock;
    }
    // 尝试与后一个空闲块合并
    puc = (uint8_t *) blockToInsert;
    if ((puc + blockToInsert->blockSize) == (uint8_t *) iterator->nextFreeBlock) {
        if (iterator->nextFreeBlock != blockLinkEnd) {
            blockToInsert->blockSize += iterator->nextFreeBlock->blockSize;
            blockToInsert->nextFreeBlock = iterator->nextFreeBlock->nextFreeBlock;
        }
    }
    // 如果没有发生前向合并，则将当前块链接到链表中
    if (iterator != blockToInsert) {
        iterator->nextFreeBlock = blockToInsert;
    }
}

/**
 * @brief RTOS内部使用的内存分配函数
 * @note  实现了首次适应(First Fit)算法来查找合适的内存块。
 * @param wantedSize 请求分配的字节数
 * @return 成功则返回分配的内存指针，失败则返回NULL
 */
static void *rtos_malloc(const size_t wantedSize) {
    BlockLink_t *block, *previousBlock, *newBlockLink;
    void *pvReturn = NULL;
    MyRTOS_Port_EnterCritical(); {
        // 如果堆尚未初始化，则进行初始化
        if (blockLinkEnd == NULL) {
            heapInit();
        }
        if ((wantedSize > 0) && ((wantedSize & blockAllocatedBit) == 0)) {
            // 计算包括管理结构和对齐后的总大小
            size_t totalSize = heapStructSize + wantedSize;
            if ((totalSize & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) != 0) {
                totalSize += (MYRTOS_HEAP_BYTE_ALIGNMENT - (totalSize & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)));
            }
            if (totalSize <= freeBytesRemaining) {
                // 遍历空闲链表，查找足够大的内存块
                previousBlock = &start;
                block = start.nextFreeBlock;
                while ((block->blockSize < totalSize) && (block->nextFreeBlock != NULL)) {
                    previousBlock = block;
                    block = block->nextFreeBlock;
                }
                // 如果找到了合适的块
                if (block != blockLinkEnd) {
                    pvReturn = (void *) (((uint8_t *) block) + heapStructSize);
                    previousBlock->nextFreeBlock = block->nextFreeBlock;
                    // 如果剩余部分足够大，则分裂成一个新的空闲块
                    if ((block->blockSize - totalSize) > HEAP_MINIMUM_BLOCK_SIZE) {
                        newBlockLink = (BlockLink_t *) (((uint8_t *) block) + totalSize);
                        newBlockLink->blockSize = block->blockSize - totalSize;
                        block->blockSize = totalSize;
                        insertBlockIntoFreeList(newBlockLink);
                    }
                    freeBytesRemaining -= block->blockSize;
                    // 标记该块为已分配
                    block->blockSize |= blockAllocatedBit;
                    block->nextFreeBlock = NULL;
                }
            }
        }
        // 如果分配失败且请求大小大于0，报告错误
        if (pvReturn == NULL && wantedSize > 0) {
            MyRTOS_ReportError(KERNEL_ERROR_MALLOC_FAILED, (void *) wantedSize);
        }
    }
    MyRTOS_Port_ExitCritical();
    return pvReturn;
}

/**
 * @brief RTOS内部使用的内存释放函数
 * @note  将释放的内存块重新插入到空闲链表中，并尝试合并。
 * @param pv 要释放的内存指针
 */
static void rtos_free(void *pv) {
    if (pv == NULL) return;
    uint8_t *puc = (uint8_t *) pv;
    BlockLink_t *link;
    // 从用户指针回退到内存块的管理结构
    puc -= heapStructSize;
    link = (BlockLink_t *) puc;
    // 检查该块是否确实是已分配状态
    if (((link->blockSize & blockAllocatedBit) != 0) && (link->nextFreeBlock == NULL)) {
        // 清除已分配标志
        link->blockSize &= ~blockAllocatedBit;
        MyRTOS_Port_EnterCritical(); {
            // 更新剩余空闲字节数并将其插回空闲链表
            freeBytesRemaining += link->blockSize;
            insertBlockIntoFreeList(link);
        }
        MyRTOS_Port_ExitCritical();
    }
}

/**
 * @brief 从一个通用双向链表中移除任务
 * @param ppListHead 指向链表头指针的指针
 * @param taskToRemove 要移除的任务句柄
 */
static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t taskToRemove) {
    if (taskToRemove == NULL) return;
    // 更新前一个节点的 next 指针
    if (taskToRemove->pPrevGeneric != NULL) {
        taskToRemove->pPrevGeneric->pNextGeneric = taskToRemove->pNextGeneric;
    } else {
        // 如果是头节点，则更新链表头
        *ppListHead = taskToRemove->pNextGeneric;
    }
    // 更新后一个节点的 prev 指针
    if (taskToRemove->pNextGeneric != NULL) {
        taskToRemove->pNextGeneric->pPrevGeneric = taskToRemove->pPrevGeneric;
    }
    // 清理任务自身的链表指针
    taskToRemove->pNextGeneric = NULL;
    taskToRemove->pPrevGeneric = NULL;
    // 如果是从就绪链表中移除，需要检查是否需要清除优先级位图中的对应位
    if (taskToRemove->state == TASK_STATE_READY) {
        if (readyTaskLists[taskToRemove->priority] == NULL) {
            topReadyPriority &= ~(1UL << taskToRemove->priority);
        }
    }
}

/**
 * @brief 动态改变任务的优先级
 * @param task 目标任务句柄
 * @param newPriority 新的优先级
 */
static void task_set_priority(TaskHandle_t task, uint8_t newPriority) {
    if (task->priority == newPriority) return;
    // 如果任务是就绪状态，需要从旧的就绪链表移到新的就绪链表
    if (task->state == TASK_STATE_READY) {
        MyRTOS_Port_EnterCritical();
        removeTaskFromList(&readyTaskLists[task->priority], task);
        task->priority = newPriority;
        addTaskToReadyList(task);
        MyRTOS_Port_ExitCritical();
    } else {
        // 如果任务不是就绪状态，直接修改优先级即可
        task->priority = newPriority;
    }
}

/**
 * @brief 将任务添加到按唤醒时间排序的延迟链表中
 * @param task 要添加的任务句柄
 */
static void addTaskToSortedDelayList(TaskHandle_t task) {
    const uint64_t wakeUpTime = task->delay;
    // 插入到链表头
    if (delayedTaskListHead == NULL || wakeUpTime < delayedTaskListHead->delay) {
        task->pNextGeneric = delayedTaskListHead;
        task->pPrevGeneric = NULL;
        if (delayedTaskListHead != NULL) delayedTaskListHead->pPrevGeneric = task;
        delayedTaskListHead = task;
    } else {
        // 遍历链表找到合适的插入位置
        Task_t *iterator = delayedTaskListHead;
        while (iterator->pNextGeneric != NULL && iterator->pNextGeneric->delay <= wakeUpTime) {
            iterator = iterator->pNextGeneric;
        }
        // 插入到链表中间或尾部
        task->pNextGeneric = iterator->pNextGeneric;
        if (iterator->pNextGeneric != NULL) iterator->pNextGeneric->pPrevGeneric = task;
        iterator->pNextGeneric = task;
        task->pPrevGeneric = iterator;
    }
}

/**
 * @brief 将任务添加到相应优先级的就绪链表末尾
 * @param task 要添加的任务句柄
 */
static void addTaskToReadyList(TaskHandle_t task) {
    if (task == NULL || task->priority >= MYRTOS_MAX_PRIORITIES) return;
    MyRTOS_Port_EnterCritical(); {
        // 设置对应优先级的位图标志
        topReadyPriority |= (1UL << task->priority);
        task->pNextGeneric = NULL;
        // 将任务添加到就绪链表末尾
        if (readyTaskLists[task->priority] == NULL) {
            readyTaskLists[task->priority] = task;
            task->pPrevGeneric = NULL;
        } else {
            Task_t *pLast = readyTaskLists[task->priority];
            while (pLast->pNextGeneric != NULL) pLast = pLast->pNextGeneric;
            pLast->pNextGeneric = task;
            task->pPrevGeneric = pLast;
        }
        // 更新任务状态
        task->state = TASK_STATE_READY;
    }
    MyRTOS_Port_ExitCritical();
}

/**
 * @brief 初始化一个事件列表
 * @param pEventList 指向要初始化的事件列表的指针
 */
static void eventListInit(EventList_t *pEventList) {
    pEventList->head = NULL;
}

/**
 * @brief 将任务按优先级插入到事件等待列表中
 * @param pEventList 目标事件列表
 * @param taskToInsert 要插入的任务
 */
static void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert) {
    taskToInsert->pEventList = pEventList;
    // 插入到链表头（如果新任务优先级最高）
    if (pEventList->head == NULL || pEventList->head->priority <= taskToInsert->priority) {
        taskToInsert->pNextEvent = pEventList->head;
        pEventList->head = taskToInsert;
    } else {
        // 遍历找到按优先级排序的正确位置
        Task_t *iterator = pEventList->head;
        while (iterator->pNextEvent != NULL && iterator->pNextEvent->priority > taskToInsert->priority) {
            iterator = iterator->pNextEvent;
        }
        taskToInsert->pNextEvent = iterator->pNextEvent;
        iterator->pNextEvent = taskToInsert;
    }
}

/**
 * @brief 从任务当前等待的事件列表中移除该任务
 * @param taskToRemove 要移除的任务
 */
static void eventListRemove(TaskHandle_t taskToRemove) {
    if (taskToRemove->pEventList == NULL) return;
    EventList_t *pEventList = taskToRemove->pEventList;
    // 如果任务是事件列表的头节点
    if (pEventList->head == taskToRemove) {
        pEventList->head = taskToRemove->pNextEvent;
    } else {
        // 遍历查找并移除任务
        Task_t *iterator = pEventList->head;
        while (iterator != NULL && iterator->pNextEvent != taskToRemove) {
            iterator = iterator->pNextEvent;
        }
        if (iterator != NULL) {
            iterator->pNextEvent = taskToRemove->pNextEvent;
        }
    }
    // 清理任务中的事件列表相关指针
    taskToRemove->pNextEvent = NULL;
    taskToRemove->pEventList = NULL;
}

/**
 * @brief 向所有已注册的内核扩展广播一个事件
 * @param pEventData 要广播的事件数据
 */
static void broadcast_event(const KernelEventData_t *pEventData) {
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i]) {
            g_extensions[i](pEventData);
        }
    }
}

/*===========================================================================*
* 公开接口实现
*===========================================================================*/

/**
 * @brief 初始化RTOS内核
 * @note  此函数必须在创建任何任务或启动调度器之前调用。
 *        它会初始化所有的内核数据结构。
 */
void MyRTOS_Init(void) {
    allTaskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    nextTaskId = 0;
    for (int i = 0; i < MYRTOS_MAX_PRIORITIES; i++) {
        readyTaskLists[i] = NULL;
    }
    delayedTaskListHead = NULL;
    topReadyPriority = 0;
}

/**
 * @brief 启动RTOS调度器
 * @note  此函数不会返回。它会创建空闲任务，然后启动第一个任务的执行。
 *        在此之后，任务调度由内核接管。
 * @param idle_task_func 空闲任务的函数指针。当没有其他任务可运行时，将执行此任务。
 */
void Task_StartScheduler(void (*idle_task_func)(void *)) {
    // 必须提供一个有效的空闲任务函数
    if (idle_task_func == NULL) {
        while (1);
    }
    // 创建空闲任务，其优先级为最低
    idleTask = Task_Create(idle_task_func, "IDLE", 128, NULL, 0);
    if (idleTask == NULL) {
        // 如果空闲任务创建失败，系统无法继续
        while (1);
    }
    // 标记调度器已启动
    g_scheduler_started = 1;
    // 手动调用一次调度以选择第一个要运行的任务
    MyRTOS_Schedule();
    // 调用移植层代码来启动调度器（通常是设置第一个任务的堆栈指针并触发SVC/PendSV）
    if (MyRTOS_Port_StartScheduler() != 0) {
        // 此处不应该被执行到
    }
}

/**
 * @brief 获取当前系统滴答计数
 * @note  此函数是线程安全的。
 * @return 返回自调度器启动以来的滴答数
 */
uint64_t MyRTOS_GetTick(void) {
    MyRTOS_Port_EnterCritical();
    const uint64_t tick_value = systemTickCount;
    MyRTOS_Port_ExitCritical();
    return tick_value;
}

/**
 * @brief 系统滴答中断处理函数
 * @note  此函数应在系统滴答定时器中断（如SysTick_Handler）中调用。
 *        它负责增加系统滴答计数，并检查是否有延迟的任务需要被唤醒。
 */
int MyRTOS_Tick_Handler(void) {
    int higherPriorityTaskWoken = 0;
    // 增加系统滴答计数
    systemTickCount++;
    const uint64_t current_tick = systemTickCount;
    // 检查延迟链表头，看是否有任务的唤醒时间已到
    while (delayedTaskListHead != NULL && delayedTaskListHead->delay <= current_tick) {
        Task_t *taskToWake = delayedTaskListHead;
        // 从延迟链表中移除
        removeTaskFromList(&delayedTaskListHead, taskToWake);
        taskToWake->delay = 0;
        // 添加到就绪链表
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) {
            higherPriorityTaskWoken = 1;
        }
    }
    // 广播滴答事件
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TICK};
    broadcast_event(&eventData);
    return higherPriorityTaskWoken;
}

/**
 * @brief 检查调度器是否正在运行
 * @return 如果调度器已启动，返回1，否则返回0
 */
uint8_t MyRTOS_Schedule_IsRunning(void) {
    return g_scheduler_started;
}

/**
 * @brief 报告一个内核级错误
 * @note  此函数通过内核扩展机制广播错误事件，允许调试工具或用户代码捕获和处理这些错误。
 * @param error_type 错误类型
 * @param p_context 与错误相关的上下文数据指针
 */
void MyRTOS_ReportError(KernelErrorType_t error_type, void *p_context) {
    KernelEventData_t eventData;
    // 最好先清空事件数据结构
    memset(&eventData, 0, sizeof(eventData));

    eventData.p_context_data = p_context;

    switch (error_type) {
        case KERNEL_ERROR_STACK_OVERFLOW:
            eventData.eventType = KERNEL_EVENT_HOOK_STACK_OVERFLOW;
            eventData.task = (TaskHandle_t) p_context;
            break;

        case KERNEL_ERROR_MALLOC_FAILED:
            eventData.eventType = KERNEL_EVENT_HOOK_MALLOC_FAILED;
            eventData.mem.size = (size_t) p_context;
            eventData.mem.ptr = NULL;
            break;

        case KERNEL_ERROR_HARD_FAULT:
            eventData.eventType = KERNEL_EVENT_ERROR_HARD_FAULT;
            break;
        default:
            return; // 未知的错误类型，不处理
    }

    broadcast_event(&eventData);
}

/**
 * @brief 调度并选择下一个要运行的任务
 * @note  这是RTOS调度的核心。它会找到最高优先级的就绪任务并将其设置为当前任务。
 *        此函数被PendSV中断服务程序调用以执行上下文切换。
 * @return 返回下一个要运行任务的堆栈指针 (SP)
 */
void *schedule_next_task(void) {
    TaskHandle_t prevTask = currentTask;
    TaskHandle_t nextTaskToRun = NULL;
    // 广播任务切出事件
    if (prevTask) {
        KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_SWITCH_OUT, .task = prevTask};
        broadcast_event(&eventData);
    }
    // 如果没有就绪任务，则选择空闲任务
    if (topReadyPriority == 0) {
        nextTaskToRun = idleTask;
    } else {
        // 找到最高优先级的就绪任务
        // `__builtin_clz` 是一个GCC/Clang内置函数，用于计算前导零的数量，可以高效地找到最高置位
        uint32_t highestPriority = 31 - __builtin_clz(topReadyPriority);
        nextTaskToRun = readyTaskLists[highestPriority];
        // 实现同优先级任务的轮转调度 (Round-Robin)
        if (nextTaskToRun != NULL && nextTaskToRun->pNextGeneric != NULL) {
            // 将当前头节点移动到链表尾部
            readyTaskLists[highestPriority] = nextTaskToRun->pNextGeneric;
            nextTaskToRun->pNextGeneric->pPrevGeneric = NULL;
            Task_t *pLast = readyTaskLists[highestPriority];
            if (pLast != NULL) {
                while (pLast->pNextGeneric != NULL) pLast = pLast->pNextGeneric;
                pLast->pNextGeneric = nextTaskToRun;
                nextTaskToRun->pPrevGeneric = pLast;
                nextTaskToRun->pNextGeneric = NULL;
            } else {
                // 如果链表中只有一个元素，则它仍然是头
                readyTaskLists[highestPriority] = nextTaskToRun;
                nextTaskToRun->pPrevGeneric = NULL;
                nextTaskToRun->pNextGeneric = NULL;
            }
        }
    }
    // 更新当前任务
    currentTask = nextTaskToRun;
    // 广播任务切入事件
    if (currentTask) {
        KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_SWITCH_IN, .task = currentTask};
        broadcast_event(&eventData);
    }
    if (currentTask == NULL) return NULL; // 理论上不应发生，因为总有idleTask
    // 返回新任务的堆栈指针给移植层
    return currentTask->sp;
}

/**
 * @brief 手动触发一次任务调度
 * @note  通常在任务状态发生改变后（例如，从延迟变为就绪）调用，以确保最高优先级的任务能够运行。
 *        在RTOS的API内部，这通常通过 `MyRTOS_Port_Yield()` 实现。
 */
void MyRTOS_Schedule(void) {
    schedule_next_task();
}

// =============================
// Memory Management API
// =============================
/**
 * @brief 动态分配内存
 * @note  此函数是线程安全的。内部使用 `rtos_malloc` 并广播一个内存分配事件。
 * @param wantedSize 要分配的内存大小（字节）
 * @return 成功则返回指向已分配内存的指针，失败则返回NULL
 */
void *MyRTOS_Malloc(size_t wantedSize) {
    void *pv = rtos_malloc(wantedSize);
    // 广播内存分配事件
    KernelEventData_t eventData = {
        .eventType = KERNEL_EVENT_MALLOC,
        .mem = {.ptr = pv, .size = wantedSize}
    };
    broadcast_event(&eventData);
    return pv;
}

/**
 * @brief 释放先前分配的内存
 * @note  此函数是线程安全的。内部使用 `rtos_free` 并广播一个内存释放事件。
 * @param pv 要释放的内存指针，必须是通过 `MyRTOS_Malloc` 分配的
 */
void MyRTOS_Free(void *pv) {
    if (pv) {
        // 在释放前获取块大小以用于事件广播
        BlockLink_t *link = (BlockLink_t *) ((uint8_t *) pv - heapStructSize);
        KernelEventData_t eventData = {
            .eventType = KERNEL_EVENT_FREE,
            .mem = {.ptr = pv, .size = (link->blockSize & ~blockAllocatedBit)}
        };
        broadcast_event(&eventData);
    }
    rtos_free(pv);
}

/**
 * @brief 创建一个新任务
 * @param func 任务函数指针
 * @param taskName 任务名称（字符串），用于调试
 * @param stack_size 任务堆栈大小（以StackType_t为单位，通常是4字节）
 * @param param 传递给任务函数的参数
 * @param priority 任务优先级 (0是最低优先级)
 * @return 成功则返回任务句柄，失败则返回NULL
 */
TaskHandle_t Task_Create(void (*func)(void *), const char *taskName, uint16_t stack_size, void *param,
                         uint8_t priority) {
    if (priority >= MYRTOS_MAX_PRIORITIES || func == NULL) return NULL;
    // 为任务控制块（TCB）分配内存
    Task_t *t = MyRTOS_Malloc(sizeof(Task_t));
    if (t == NULL) return NULL;
    // 为任务堆栈分配内存
    StackType_t *stack = MyRTOS_Malloc(stack_size * sizeof(StackType_t));
    if (stack == NULL) {
        MyRTOS_Free(t);
        return NULL;
    }
    // 分配一个唯一的任务ID
    uint32_t newTaskId = (uint32_t) -1;
    MyRTOS_Port_EnterCritical();
    if (taskIdBitmap != 0xFFFFFFFFFFFFFFFFULL) {
        newTaskId = __builtin_ctzll(~taskIdBitmap); // 查找位图中第一个为0的位
        taskIdBitmap |= (1ULL << newTaskId);
    }
    MyRTOS_Port_ExitCritical();
    if (newTaskId == (uint32_t) -1) {
        // 如果没有可用的任务ID
        MyRTOS_Free(stack);
        MyRTOS_Free(t);
        return NULL;
    }
    // 初始化任务控制块（TCB）
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    t->taskId = newTaskId;
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
    t->taskName = taskName;
    t->stackSize_words = stack_size;
    // 使用魔法数字填充堆栈，用于堆栈溢出检测
    for (uint16_t i = 0; i < stack_size; ++i) {
        stack[i] = 0xA5A5A5A5;
    }
    // 调用移植层代码初始化任务堆栈（模拟CPU上下文）
    t->sp = MyRTOS_Port_InitialiseStack(stack + stack_size, func, param);
    MyRTOS_Port_EnterCritical(); {
        // 将新任务添加到全局任务列表中
        if (allTaskListHead == NULL) {
            allTaskListHead = t;
        } else {
            Task_t *p = allTaskListHead;
            while (p->pNextTask != NULL) p = p->pNextTask;
            p->pNextTask = t;
        }
        // 将新任务添加到就绪列表
        addTaskToReadyList(t);
    }
    MyRTOS_Port_ExitCritical();
    // 广播任务创建事件
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_CREATE, .task = t};
    broadcast_event(&eventData);
    return t;
}

/**
 * @brief 删除一个任务
 * @param task_h 要删除的任务句柄。如果为NULL，则删除当前任务。
 * @return 成功返回0，失败返回-1 (例如，尝试删除空闲任务)
 */
int Task_Delete(TaskHandle_t task_h) {
    Task_t *task_to_delete = (task_h == NULL) ? currentTask : task_h;
    // 不允许删除空闲任务
    if (task_to_delete == idleTask || task_to_delete == NULL) return -1;
    uint32_t deleted_task_id = task_to_delete->taskId;
    // 广播任务删除事件
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_DELETE, .task = task_to_delete};
    broadcast_event(&eventData);
    MyRTOS_Port_EnterCritical(); {
        // 从其所在的任何链表中移除任务
        if (task_to_delete->state == TASK_STATE_READY) {
            removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
        } else if (task_to_delete->state == TASK_STATE_DELAYED || task_to_delete->state == TASK_STATE_BLOCKED) {
            removeTaskFromList(&delayedTaskListHead, task_to_delete);
        }
        // 如果任务正在等待事件，也从事件列表中移除
        if (task_to_delete->pEventList != NULL) {
            eventListRemove(task_to_delete);
        }
        // 释放任务持有的所有互斥锁
        while (task_to_delete->held_mutexes_head != NULL) {
            Mutex_Unlock(task_to_delete->held_mutexes_head);
        }
        task_to_delete->state = TASK_STATE_UNUSED;
        // 从全局任务列表中移除
        Task_t *prev = NULL, *curr = allTaskListHead;
        while (curr != NULL && curr != task_to_delete) {
            prev = curr;
            curr = curr->pNextTask;
        }
        if (curr != NULL) {
            if (prev == NULL) allTaskListHead = curr->pNextTask;
            else prev->pNextTask = curr->pNextTask;
        }
        void *stack_to_free = task_to_delete->stack_base;
        // 如果是删除自身
        if (task_h == NULL) {
            currentTask = NULL; // 标记当前任务为空，调度器将选择新任务
            taskIdBitmap &= ~(1ULL << deleted_task_id); // 回收任务ID
            MyRTOS_Free(task_to_delete);
            MyRTOS_Free(stack_to_free);
            MyRTOS_Port_ExitCritical();
            MyRTOS_Port_Yield(); // 触发调度，此任务将不再执行
        } else {
            // 如果是删除其他任务
            taskIdBitmap &= ~(1ULL << deleted_task_id); // 回收任务ID
            MyRTOS_Free(task_to_delete);
            MyRTOS_Free(stack_to_free);
            MyRTOS_Port_ExitCritical();
        }
    }
    return 0;
}

/**
 * @brief 将当前任务延迟指定的滴答数
 * @param tick 要延迟的系统滴答数
 */
void Task_Delay(uint32_t tick) {
    if (tick == 0 || g_scheduler_started == 0) return;
    MyRTOS_Port_EnterCritical(); {
        // 从就绪链表中移除当前任务
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        // 计算唤醒时间并设置任务状态
        currentTask->delay = MyRTOS_GetTick() + tick;
        currentTask->state = TASK_STATE_DELAYED;
        // 将任务添加到排序的延迟链表中
        addTaskToSortedDelayList(currentTask);
    }
    MyRTOS_Port_ExitCritical();
    // 触发调度
    MyRTOS_Port_Yield();
}

/**
 * @brief 向一个任务发送通知，唤醒正在等待的任务
 * @param task_h 目标任务的句柄
 * @return 成功返回0
 */
int Task_Notify(TaskHandle_t task_h) {
    int trigger_yield = 0;
    MyRTOS_Port_EnterCritical(); {
        // 检查目标任务是否正在等待通知
        if (task_h->is_waiting_notification && task_h->state == TASK_STATE_BLOCKED) {
            task_h->is_waiting_notification = 0;
            addTaskToReadyList(task_h);
            // 如果被唤醒的任务优先级更高，则需要进行调度
            if (task_h->priority > currentTask->priority) {
                trigger_yield = 1;
            }
        }
    }
    MyRTOS_Port_ExitCritical();
    if (trigger_yield)
        MyRTOS_Port_Yield();
    return 0;
}

/**
 * @brief 从中断服务程序(ISR)中向任务发送通知
 * @param task_h 目标任务的句柄
 * @param higherPriorityTaskWoken 指针，用于返回是否有更高优先级的任务被唤醒
 * @return 成功返回0，失败返回-1
 */
int Task_NotifyFromISR(TaskHandle_t task_h, int *higherPriorityTaskWoken) {
    if (higherPriorityTaskWoken == NULL) return -1;
    *higherPriorityTaskWoken = 0;
    MyRTOS_Port_EnterCritical(); {
        if (task_h->is_waiting_notification && task_h->state == TASK_STATE_BLOCKED) {
            task_h->is_waiting_notification = 0;
            addTaskToReadyList(task_h);
            if (task_h->priority > currentTask->priority) {
                *higherPriorityTaskWoken = 1;
            }
        }
    }
    MyRTOS_Port_ExitCritical();
    return 0;
}

/**
 * @brief 使当前任务进入阻塞状态，等待通知
 * @note  任务将一直阻塞，直到 `Task_Notify` 或 `Task_NotifyFromISR` 被调用
 */
void Task_Wait(void) {
    MyRTOS_Port_EnterCritical(); {
        // 从就绪链表移除
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        // 设置等待通知标志和阻塞状态
        currentTask->is_waiting_notification = 1;
        currentTask->state = TASK_STATE_BLOCKED;
    }
    MyRTOS_Port_ExitCritical();
    // 触发调度
    MyRTOS_Port_Yield();
}

/**
 * @brief 获取任务的当前状态
 * @param task_h 目标任务的句柄
 * @return 返回任务的状态 (TaskState_t)
 */
TaskState_t Task_GetState(TaskHandle_t task_h) {
    return task_h ? ((Task_t *) task_h)->state : TASK_STATE_UNUSED;
}

/**
 * @brief 获取任务的当前优先级
 * @param task_h 目标任务的句柄
 * @return 返回任务的优先级
 */
uint8_t Task_GetPriority(TaskHandle_t task_h) {
    return task_h ? ((Task_t *) task_h)->priority : 0;
}

/**
 * @brief 获取当前正在运行的任务的句柄
 * @return 返回当前任务的句柄
 */
TaskHandle_t Task_GetCurrentTaskHandle(void) {
    return currentTask;
}

/**
 * @brief 获取任务的唯一ID
 * @param task_h 目标任务的句柄
 * @return 返回任务的ID
 */
uint32_t Task_GetId(TaskHandle_t task_h) {
    if (!task_h) return (uint32_t) -1;
    return ((Task_t *) task_h)->taskId;
}

/**
 * @brief 创建一个消息队列
 * @param length 队列能够存储的最大项目数
 * @param itemSize 每个项目的大小（字节）
 * @return 成功则返回队列句柄，失败则返回NULL
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize) {
    if (length == 0 || itemSize == 0) return NULL;
    // 分配队列控制结构内存
    Queue_t *queue = MyRTOS_Malloc(sizeof(Queue_t));
    if (queue == NULL) return NULL;
    // 分配队列存储区内存
    queue->storage = (uint8_t *) MyRTOS_Malloc(length * itemSize);
    if (queue->storage == NULL) {
        MyRTOS_Free(queue);
        return NULL;
    }
    // 初始化队列属性
    queue->length = length;
    queue->itemSize = itemSize;
    queue->waitingCount = 0;
    queue->writePtr = queue->storage;
    queue->readPtr = queue->storage;
    eventListInit(&queue->sendEventList); // 初始化等待发送的任务列表
    eventListInit(&queue->receiveEventList); // 初始化等待接收的任务列表
    return queue;
}

/**
 * @brief 删除一个消息队列
 * @note  会唤醒所有等待该队列的任务。
 * @param delQueue 要删除的队列句柄
 */
void Queue_Delete(QueueHandle_t delQueue) {
    Queue_t *queue = delQueue;
    if (queue == NULL) return;
    MyRTOS_Port_EnterCritical(); {
        // 唤醒所有等待发送的任务
        while (queue->sendEventList.head != NULL) {
            Task_t *taskToWake = queue->sendEventList.head;
            eventListRemove(taskToWake);
            addTaskToReadyList(taskToWake);
        }
        // 唤醒所有等待接收的任务
        while (queue->receiveEventList.head != NULL) {
            Task_t *taskToWake = queue->receiveEventList.head;
            eventListRemove(taskToWake);
            addTaskToReadyList(taskToWake);
        }
        // 释放内存
        MyRTOS_Free(queue->storage);
        MyRTOS_Free(queue);
    }
    MyRTOS_Port_ExitCritical();
}

/**
 * @brief 向队列发送一个项目
 * @param queue 目标队列句柄
 * @param item 指向要发送的项目的指针
 * @param block_ticks 如果队列已满，任务将阻塞等待的最大滴答数。0表示不等待，MYRTOS_MAX_DELAY表示永久等待。
 * @return 成功发送返回1，失败或超时返回0
 */
int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) return 0;
    while (1) {
        MyRTOS_Port_EnterCritical();
        // 情况1: 有任务正在等待接收数据
        if (pQueue->receiveEventList.head != NULL) {
            Task_t *taskToWake = pQueue->receiveEventList.head;
            eventListRemove(taskToWake);
            // 如果该任务同时也在延迟列表中，从中移除
            if (taskToWake->delay > 0) {
                removeTaskFromList(&delayedTaskListHead, taskToWake);
                taskToWake->delay = 0;
            }
            // 直接将数据拷贝给等待的任务
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            taskToWake->eventData = NULL;
            addTaskToReadyList(taskToWake);
            // 如果被唤醒的任务优先级更高，触发调度
            if (taskToWake->priority > currentTask->priority)
                MyRTOS_Port_Yield();
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // 情况2: 队列未满
        if (pQueue->waitingCount < pQueue->length) {
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            // 写指针回环
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // 情况3: 队列已满，且不允许阻塞
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
        // 情况4: 队列已满，需要阻塞
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&pQueue->sendEventList, currentTask); // 加入发送等待列表
        currentTask->delay = 0;
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_ExitCritical();
        MyRTOS_Port_Yield(); // 触发调度，任务进入阻塞
        // 任务被唤醒后，检查是否是正常唤醒（而不是超时）
        if (currentTask->pEventList == NULL) continue; // 如果是正常唤醒，pEventList会被设为NULL，循环重试发送
        // 如果是超时唤醒
        MyRTOS_Port_EnterCritical();
        eventListRemove(currentTask);
        MyRTOS_Port_ExitCritical();
        return 0;
    }
}

/**
 * @brief 从队列接收一个项目
 * @param queue 目标队列句柄
 * @param buffer 用于存储接收到的项目的缓冲区指针
 * @param block_ticks 如果队列为空，任务将阻塞等待的最大滴答数。0表示不等待，MYRTOS_MAX_DELAY表示永久等待。
 * @return 成功接收返回1，失败或超时返回0
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) return 0;
    while (1) {
        MyRTOS_Port_EnterCritical();
        // 情况1: 队列中有数据
        if (pQueue->waitingCount > 0) {
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            // 读指针回环
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;
            // 如果有任务在等待发送，唤醒一个
            if (pQueue->sendEventList.head != NULL) {
                Task_t *taskToWake = pQueue->sendEventList.head;
                eventListRemove(taskToWake);
                // 如果该任务同时也在延迟列表中，从中移除
                if (taskToWake->delay > 0) {
                    removeTaskFromList(&delayedTaskListHead, taskToWake);
                    taskToWake->delay = 0;
                }
                addTaskToReadyList(taskToWake);
                // 如果被唤醒的任务优先级更高，触发调度
                if (taskToWake->priority > currentTask->priority)
                    MyRTOS_Port_Yield();
            }
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // 情况2: 队列为空，且不允许阻塞
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
        // 情况3: 队列为空，需要阻塞
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        currentTask->eventData = buffer; // 临时存储接收缓冲区指针
        eventListInsert(&pQueue->receiveEventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_ExitCritical();
        MyRTOS_Port_Yield(); // 触发调度，任务进入阻塞
        // 任务被唤醒后，检查是否是正常唤醒（数据已被直接拷贝到buffer）
        if (currentTask->pEventList == NULL) return 1;
        // 如果是超时唤醒
        MyRTOS_Port_EnterCritical();
        eventListRemove(currentTask);
        currentTask->eventData = NULL;
        MyRTOS_Port_ExitCritical();
        return 0;
    }
}

// =============================
// Mutex Management API
// =============================
/**
 * @brief 创建一个互斥锁
 * @return 成功则返回互斥锁句柄，失败则返回NULL
 */
MutexHandle_t Mutex_Create(void) {
    Mutex_t *mutex = MyRTOS_Malloc(sizeof(Mutex_t));
    if (mutex != NULL) {
        mutex->locked = 0;
        mutex->owner_tcb = NULL;
        mutex->next_held_mutex = NULL;
        mutex->recursion_count = 0;
        eventListInit(&mutex->eventList);
    }
    return mutex;
}

/**
 * @brief 尝试获取一个互斥锁，带超时
 * @param mutex 目标互斥锁句柄
 * @param block_ticks 如果锁已被占用，任务将阻塞等待的最大滴答数。0表示不等待，MYRTOS_MAX_DELAY表示永久等待。
 * @return 成功获取返回1，失败或超时返回0
 */
int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks) {
    while (1) {
        MyRTOS_Port_EnterCritical();
        // 情况1: 锁未被占用，成功获取
        if (!mutex->locked) {
            mutex->locked = 1;
            mutex->owner_tcb = currentTask;
            // 将此互斥锁加入当前任务持有的互斥锁链表中
            mutex->next_held_mutex = currentTask->held_mutexes_head;
            currentTask->held_mutexes_head = mutex;
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // 情况2: 锁已被占用，且不允许阻塞
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
        // 情况3: 锁已被占用，需要阻塞
        // 实现优先级继承：如果当前任务优先级高于锁的持有者，则提升持有者的优先级
        TaskHandle_t owner_tcb = mutex->owner_tcb;
        if (owner_tcb != NULL && currentTask->priority > owner_tcb->priority) {
            task_set_priority(owner_tcb, currentTask->priority);
        }
        // 将当前任务从就绪列表移除，并加入互斥锁的等待列表
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&mutex->eventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_ExitCritical();
        MyRTOS_Port_Yield(); // 触发调度，进入阻塞
        // 任务被唤醒后
        if (mutex->owner_tcb == currentTask) return 1; // 检查是否已成为新的持有者
        // 如果是超时唤醒
        if (currentTask->pEventList != NULL) {
            MyRTOS_Port_EnterCritical();
            eventListRemove(currentTask);
            MyRTOS_Port_ExitCritical();
            return 0;
        }
    }
}

/**
 * @brief 获取一个互斥锁（永久等待）
 * @param mutex 目标互斥锁句柄
 */
void Mutex_Lock(MutexHandle_t mutex) {
    Mutex_Lock_Timeout(mutex, MYRTOS_MAX_DELAY);
}

/**
 * @brief 释放一个互斥锁
 * @param mutex 目标互斥锁句柄
 */
void Mutex_Unlock(MutexHandle_t mutex) {
    int trigger_yield = 0;
    MyRTOS_Port_EnterCritical();
    // 检查是否是锁的持有者
    if (!mutex->locked || mutex->owner_tcb != currentTask) {
        MyRTOS_Port_ExitCritical();
        return;
    }
    // 从当前任务持有的互斥锁链表中移除此锁
    if (currentTask->held_mutexes_head == mutex) {
        currentTask->held_mutexes_head = mutex->next_held_mutex;
    } else {
        Mutex_t *p_iterator = currentTask->held_mutexes_head;
        while (p_iterator != NULL && p_iterator->next_held_mutex != mutex) p_iterator = p_iterator->next_held_mutex;
        if (p_iterator != NULL) p_iterator->next_held_mutex = mutex->next_held_mutex;
    }
    mutex->next_held_mutex = NULL;
    // 优先级恢复：将任务优先级恢复到其基础优先级，或其仍然持有的其他互斥锁所要求的最高优先级
    uint8_t new_priority = currentTask->basePriority;
    Mutex_t *p_held_mutex = currentTask->held_mutexes_head;
    while (p_held_mutex != NULL) {
        if (p_held_mutex->eventList.head != NULL && p_held_mutex->eventList.head->priority > new_priority) {
            new_priority = p_held_mutex->eventList.head->priority;
        }
        p_held_mutex = p_held_mutex->next_held_mutex;
    }
    task_set_priority(currentTask, new_priority);
    // 标记锁为未锁定
    mutex->locked = 0;
    mutex->owner_tcb = NULL;
    // 如果有任务在等待此锁，则唤醒优先级最高的那个
    if (mutex->eventList.head != NULL) {
        Task_t *taskToWake = mutex->eventList.head;
        eventListRemove(taskToWake);
        // 将锁的所有权直接转移给被唤醒的任务
        mutex->locked = 1;
        mutex->owner_tcb = taskToWake;
        mutex->next_held_mutex = taskToWake->held_mutexes_head;
        taskToWake->held_mutexes_head = mutex;
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) trigger_yield = 1;
    }
    MyRTOS_Port_ExitCritical();
    if (trigger_yield)
        MyRTOS_Port_Yield();
}

/**
 * @brief 递归地获取一个互斥锁
 * @note  如果当前任务已是该锁的持有者，则增加递归计数；否则，行为与 `Mutex_Lock` 相同。
 * @param mutex 目标互斥锁句柄
 */
void Mutex_Lock_Recursive(MutexHandle_t mutex) {
    MyRTOS_Port_EnterCritical();
    // 如果已经持有该锁，增加递归计数
    if (mutex->locked && mutex->owner_tcb == currentTask) {
        mutex->recursion_count++;
        MyRTOS_Port_ExitCritical();
        return;
    }
    MyRTOS_Port_ExitCritical();
    // 首次获取该锁
    Mutex_Lock(mutex);
    MyRTOS_Port_EnterCritical();
    if (mutex->owner_tcb == currentTask) mutex->recursion_count = 1;
    MyRTOS_Port_ExitCritical();
}

/**
 * @brief 递归地释放一个互斥锁
 * @note  如果递归计数大于1，则仅递减计数；如果计数为1，则完全释放该锁。
 * @param mutex 目标互斥锁句柄
 */
void Mutex_Unlock_Recursive(MutexHandle_t mutex) {
    MyRTOS_Port_EnterCritical();
    if (mutex->locked && mutex->owner_tcb == currentTask) {
        mutex->recursion_count--;
        if (mutex->recursion_count == 0) {
            // 当递归计数归零时，才真正释放锁
            MyRTOS_Port_ExitCritical();
            Mutex_Unlock(mutex);
        } else {
            MyRTOS_Port_ExitCritical();
        }
    } else {
        MyRTOS_Port_ExitCritical();
    }
}

/**
 * @brief 创建一个计数信号量
 * @param maxCount 信号量的最大计数值
 * @param initialCount 信号量的初始计数值
 * @return 成功则返回信号量句柄，失败则返回NULL
 */
SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount) {
    if (maxCount == 0 || initialCount > maxCount) return NULL;
    Semaphore_t *semaphore = MyRTOS_Malloc(sizeof(Semaphore_t));
    if (semaphore != NULL) {
        semaphore->count = initialCount;
        semaphore->maxCount = maxCount;
        eventListInit(&semaphore->eventList);
    }
    return semaphore;
}

/**
 * @brief 删除一个信号量
 * @param semaphore 要删除的信号量句柄
 */
void Semaphore_Delete(SemaphoreHandle_t semaphore) {
    if (semaphore == NULL) return;
    MyRTOS_Port_EnterCritical();
    // 唤醒所有等待该信号量的任务
    while (semaphore->eventList.head != NULL) {
        Task_t *taskToWake = semaphore->eventList.head;
        eventListRemove(taskToWake);
        addTaskToReadyList(taskToWake);
    }
    MyRTOS_Free(semaphore);
    MyRTOS_Port_ExitCritical();
}

/**
 * @brief 获取（P操作）一个信号量
 * @param semaphore 目标信号量句柄
 * @param block_ticks 如果信号量计数为0，任务将阻塞等待的最大滴答数。0表示不等待，MYRTOS_MAX_DELAY表示永久等待。
 * @return 成功获取返回1，失败或超时返回0
 */
int Semaphore_Take(SemaphoreHandle_t semaphore, uint32_t block_ticks) {
    if (semaphore == NULL) return 0;
    while (1) {
        MyRTOS_Port_EnterCritical();
        // 情况1: 信号量计数大于0，成功获取
        if (semaphore->count > 0) {
            semaphore->count--;
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // 情况2: 信号量为0，且不阻塞
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
        // 情况3: 信号量为0，需要阻塞
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&semaphore->eventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_ExitCritical();
        MyRTOS_Port_Yield(); // 触发调度，进入阻塞
        // 被唤醒后，检查是否是正常唤醒
        if (currentTask->pEventList == NULL) return 1;
        // 如果是超时唤醒
        MyRTOS_Port_EnterCritical();
        eventListRemove(currentTask);
        MyRTOS_Port_ExitCritical();
        return 0;
    }
}

/**
 * @brief 释放（V操作）一个信号量
 * @param semaphore 目标信号量句柄
 * @return 成功释放返回1，失败（例如信号量已达最大值）返回0
 */
int Semaphore_Give(SemaphoreHandle_t semaphore) {
    if (semaphore == NULL) return 0;
    int trigger_yield = 0;
    MyRTOS_Port_EnterCritical();
    // 如果有任务在等待信号量，则直接唤醒一个，而不增加计数值
    if (semaphore->eventList.head != NULL) {
        Task_t *taskToWake = semaphore->eventList.head;
        eventListRemove(taskToWake);
        if (taskToWake->delay > 0) {
            removeTaskFromList(&delayedTaskListHead, taskToWake);
            taskToWake->delay = 0;
        }
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) trigger_yield = 1;
    } else {
        // 如果没有任务等待，则增加计数值
        if (semaphore->count < semaphore->maxCount) {
            semaphore->count++;
        } else {
            // 已达最大值，释放失败
            MyRTOS_Port_ExitCritical();
            return 0;
        }
    }
    MyRTOS_Port_ExitCritical();
    if (trigger_yield)
        MyRTOS_Port_Yield();
    return 1;
}

/**
 * @brief 从中断服务程序(ISR)中释放一个信号量
 * @param semaphore 目标信号量句柄
 * @param pxHigherPriorityTaskWoken 指针，用于返回是否有更高优先级的任务被唤醒
 * @return 成功返回1，失败返回0
 */
int Semaphore_GiveFromISR(SemaphoreHandle_t semaphore, int *pxHigherPriorityTaskWoken) {
    if (semaphore == NULL || pxHigherPriorityTaskWoken == NULL) return 0;
    *pxHigherPriorityTaskWoken = 0;
    int result = 0;
    MyRTOS_Port_EnterCritical();
    // 逻辑与 Semaphore_Give 类似
    if (semaphore->eventList.head != NULL) {
        Task_t *taskToWake = semaphore->eventList.head;
        eventListRemove(taskToWake);
        if (taskToWake->delay > 0) {
            removeTaskFromList(&delayedTaskListHead, taskToWake);
            taskToWake->delay = 0;
        }
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) {
            *pxHigherPriorityTaskWoken = 1;
        }
        result = 1;
    } else {
        if (semaphore->count < semaphore->maxCount) {
            semaphore->count++;
            result = 1;
        }
    }
    MyRTOS_Port_ExitCritical();
    return result;
}

/**
 * @brief 注册一个内核扩展回调函数
 * @note  内核扩展可用于调试、跟踪或实现自定义功能，它会在特定内核事件发生时被调用。
 * @param callback 要注册的回调函数指针
 * @return 成功返回0，失败返回-1
 */
int MyRTOS_RegisterExtension(KernelExtensionCallback_t callback) {
    MyRTOS_Port_EnterCritical();
    // 检查扩展槽是否已满或回调函数是否为空
    if (g_extension_count >= MAX_KERNEL_EXTENSIONS || callback == NULL) {
        MyRTOS_Port_ExitCritical();
        return -1;
    }
    // 避免重复注册
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i] == callback) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
    }
    // 添加新的回调
    g_extensions[g_extension_count++] = callback;
    MyRTOS_Port_ExitCritical();
    return 0;
}

/**
 * @brief 注销一个内核扩展回调函数
 * @param callback 要注销的回调函数指针
 * @return 成功返回0，失败（未找到该回调）返回-1
 */
int MyRTOS_UnregisterExtension(KernelExtensionCallback_t callback) {
    int found = 0;
    MyRTOS_Port_EnterCritical();
    if (callback == NULL) {
        MyRTOS_Port_ExitCritical();
        return -1;
    }
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i] == callback) {
            // 将最后一个元素移到当前位置，然后缩减计数
            g_extensions[i] = g_extensions[g_extension_count - 1];
            g_extensions[g_extension_count - 1] = NULL;
            g_extension_count--;
            found = 1;
            break;
        }
    }
    MyRTOS_Port_ExitCritical();
    return found ? 0 : -1;
}
