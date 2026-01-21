/**
 * @file myrtos_scheduler.c
 * @brief MyRTOS 调度器和事件列表模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 私有变量
 *===========================================================================*/

// 按优先级组织的就绪任务链表数组
static TaskHandle_t readyTaskLists[MYRTOS_MAX_PRIORITIES];
// 延迟任务链表的头
static TaskHandle_t delayedTaskListHead = NULL;
// 一个位图，用于快速查找当前存在的最高优先级的就绪任务
static volatile uint32_t topReadyPriority = 0;

/*===========================================================================*
 * 内部函数实现
 *===========================================================================*/

/**
 * @brief 从一个通用双向链表中移除任务
 * @param ppListHead 指向链表头指针的指针
 * @param taskToRemove 要移除的任务句柄
 */
void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t taskToRemove) {
    if (taskToRemove == NULL)
        return;
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
 * @brief 将任务添加到按唤醒时间排序的延迟链表中
 * @param task 要添加的任务句柄
 */
void addTaskToSortedDelayList(TaskHandle_t task) {
    const uint64_t wakeUpTime = task->delay;
    // 插入到链表头
    if (delayedTaskListHead == NULL || wakeUpTime < delayedTaskListHead->delay) {
        task->pNextGeneric = delayedTaskListHead;
        task->pPrevGeneric = NULL;
        if (delayedTaskListHead != NULL)
            delayedTaskListHead->pPrevGeneric = task;
        delayedTaskListHead = task;
    } else {
        // 遍历链表找到合适的插入位置
        Task_t *iterator = delayedTaskListHead;
        while (iterator->pNextGeneric != NULL && iterator->pNextGeneric->delay <= wakeUpTime) {
            iterator = iterator->pNextGeneric;
        }
        // 插入到链表中间或尾部
        task->pNextGeneric = iterator->pNextGeneric;
        if (iterator->pNextGeneric != NULL)
            iterator->pNextGeneric->pPrevGeneric = task;
        iterator->pNextGeneric = task;
        task->pPrevGeneric = iterator;
    }
}

/**
 * @brief 将任务添加到相应优先级的就绪链表末尾
 * @param task 要添加的任务句柄
 */
void addTaskToReadyList(TaskHandle_t task) {
    if (task == NULL || task->priority >= MYRTOS_MAX_PRIORITIES)
        return;
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
            while (pLast->pNextGeneric != NULL)
                pLast = pLast->pNextGeneric;
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
void eventListInit(EventList_t *pEventList) {
    pEventList->head = NULL;
}

/**
 * @brief 将任务按优先级插入到事件等待列表中
 * @param pEventList 目标事件列表
 * @param taskToInsert 要插入的任务
 */
void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert) {
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
void eventListRemove(TaskHandle_t taskToRemove) {
    if (taskToRemove->pEventList == NULL)
        return;
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
 * @brief 动态改变任务的优先级
 * @param task 目标任务句柄
 * @param newPriority 新的优先级
 */
void task_set_priority(TaskHandle_t task, uint8_t newPriority) {
    if (task->priority == newPriority)
        return;
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

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

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
                while (pLast->pNextGeneric != NULL)
                    pLast = pLast->pNextGeneric;
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
    if (currentTask == NULL)
        return NULL; // 理论上不应发生，因为总有idleTask
    // 返回新任务的堆栈指针给移植层
    return currentTask->sp;
}

/**
 * @brief 手动触发一次任务调度
 * @note  通常在任务状态发生改变后（例如，从延迟变为就绪）调用，以确保最高优先级的任务能够运行。
 *        在RTOS的API内部，这通常通过 `MyRTOS_Port_Yield()` 实现。
 */
void MyRTOS_Schedule(void) { schedule_next_task(); }

/**
 * @brief 初始化调度器内部状态
 * @note  由 MyRTOS_Init 调用
 */
void scheduler_init(void) {
    for (int i = 0; i < MYRTOS_MAX_PRIORITIES; i++) {
        readyTaskLists[i] = NULL;
    }
    delayedTaskListHead = NULL;
    topReadyPriority = 0;
}

/**
 * @brief 获取延迟任务链表头（内部使用）
 */
TaskHandle_t *get_delayed_task_list_head(void) {
    return &delayedTaskListHead;
}

/**
 * @brief 获取就绪任务链表（内部使用）
 */
TaskHandle_t *get_ready_task_list(uint8_t priority) {
    if (priority >= MYRTOS_MAX_PRIORITIES)
        return NULL;
    return &readyTaskLists[priority];
}
