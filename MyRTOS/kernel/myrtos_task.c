/**
 * @file myrtos_task.c
 * @brief MyRTOS 任务管理模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 外部函数声明
 *===========================================================================*/
extern void task_set_priority(TaskHandle_t task, uint8_t newPriority);
extern TaskHandle_t *get_delayed_task_list_head(void);
extern TaskHandle_t *get_ready_task_list(uint8_t priority);

/*===========================================================================*
 * 私有变量
 *===========================================================================*/

// 用于分配下一个任务ID的计数器
static uint32_t nextTaskId = 0;
// 任务ID的位图，用于快速查找可用的ID
static uint64_t taskIdBitmap = 0;

/*===========================================================================*
 * 私有函数
 *===========================================================================*/

/**
 * @brief 所有任务的实际入口包装函数
 * @note  如果任务函数返回了,这里可以兜底, 防止系统崩溃
 * @param parameters  指向TaskEntryPoint_t的指针
 */
static void taskWrapper(void *wrapperPrameters) {
    TaskEntryPoint_t *entry = (TaskEntryPoint_t *) wrapperPrameters;
    // 从结构体中提取出真正的用户函数和参数
    void (*taskFunc)(void *) = entry->taskFunc;
    void *parameters = entry->parameters;
    MyRTOS_Free(entry);
    // 调用真正的用户任务函数
    taskFunc(parameters);
    // 如果用户任务函数执行到这,捕获它,并调用 Task_Delete 来删除
    //NULL 表示删除当前正在运行的任务
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_ERROR_TASK_RETURN, .task = currentTask};
    broadcast_event(&eventData);
    Task_Delete(NULL);
    while (1); // 永远不会执行
}

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

/**
 * @brief 创建一个新任务
 * @param func 任务函数指针
 * @param taskName 任务名称-字符串
 * @param stack_size 任务堆栈大小（以StackType_t为单位，通常是4字节）
 * @param param 传递给任务函数的参数
 * @param priority 任务优先级 (0是最低优先级)
 * @return 成功则返回任务句柄，失败则返回NULL
 */
TaskHandle_t Task_Create(void (*func)(void *), const char *taskName, uint16_t stack_size, void *param,
                         uint8_t priority) {
    if (priority >= MYRTOS_MAX_PRIORITIES || func == NULL)
        return NULL;
    // 为任务控制块（TCB）分配内存
    Task_t *t = MyRTOS_Malloc(sizeof(Task_t));
    if (t == NULL)
        return NULL;
    // 为任务堆栈分配内存
    StackType_t *stack = MyRTOS_Malloc(stack_size * sizeof(StackType_t));
    if (stack == NULL) {
        MyRTOS_Free(t);
        return NULL;
    }

    TaskEntryPoint_t *entry = MyRTOS_Malloc(sizeof(TaskEntryPoint_t));
    if (entry == NULL) {
        MyRTOS_Free(stack);
        MyRTOS_Free(t);
        return NULL;
    }
    // 填充
    entry->taskFunc = func;
    entry->parameters = param;

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
    t->signals_pending = 0;
    t->signals_wait_mask = 0;
    t->wait_options = 0;
    eventListInit(&t->signal_event_list);
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
    char *name_buffer = NULL;
    char default_name_temp[16];
    if (taskName != NULL && *taskName != '\0') {
        // 如果提供了有效的名字
        size_t name_len = strlen(taskName) + 1;
        name_buffer = MyRTOS_Malloc(name_len);
        if (name_buffer != NULL) {
            // 内存分配成功，复制名字
            memcpy(name_buffer, taskName, name_len);
        }
    }

    if (name_buffer == NULL) {
        // 如果 (1) taskName是NULL (2) taskName是空字符串 (3) Malloc失败
        // 我们都需要创建一个默认名字
        snprintf(default_name_temp, sizeof(default_name_temp), "Unnamed_%lu", newTaskId);
        // 为默认名字分配内存并复制
        size_t default_len = strlen(default_name_temp) + 1;
        name_buffer = MyRTOS_Malloc(default_len);
        if (name_buffer != NULL) {
            memcpy(name_buffer, default_name_temp, default_len);
        } else {
            MyRTOS_Free(stack);
            MyRTOS_Free(t);
            return NULL;
        }
    }
    if (name_buffer != NULL) {
        t->taskName = name_buffer;
    }
    t->stackSize_words = stack_size;
    // 使用魔法数字填充堆栈，用于堆栈溢出检测
    for (uint16_t i = 0; i < stack_size; ++i) {
        stack[i] = 0xA5A5A5A5;
    }
    // 调用移植层代码初始化任务堆栈（模拟CPU上下文）
    t->sp = MyRTOS_Port_InitialiseStack(stack + stack_size, taskWrapper, entry);
    MyRTOS_Port_EnterCritical(); {
        // 将新任务添加到全局任务列表中
        if (allTaskListHead == NULL) {
            allTaskListHead = t;
        } else {
            Task_t *p = allTaskListHead;
            while (p->pNextTask != NULL)
                p = p->pNextTask;
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
    if (task_to_delete == idleTask || task_to_delete == NULL)
        return -1;
    uint32_t deleted_task_id = task_to_delete->taskId;
    // 广播任务删除事件
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_DELETE, .task = task_to_delete};
    broadcast_event(&eventData);
    MyRTOS_Port_EnterCritical();
    // 从其所在的任何链表中移除任务
    if (task_to_delete->state == TASK_STATE_READY) {
        removeTaskFromList(get_ready_task_list(task_to_delete->priority), task_to_delete);
    } else if (task_to_delete->state == TASK_STATE_DELAYED || task_to_delete->state == TASK_STATE_BLOCKED) {
        if(task_to_delete->delay > 0) {
            removeTaskFromList(get_delayed_task_list_head(), task_to_delete);
        }
        if(task_to_delete->pEventList != NULL) {
            eventListRemove(task_to_delete);
        }
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
        if (prev == NULL)
            allTaskListHead = curr->pNextTask;
        else
            prev->pNextTask = curr->pNextTask;
    }
    void *stack_to_free = task_to_delete->stack_base;
    if (task_to_delete->taskName != NULL) {
        MyRTOS_Free((void *) task_to_delete->taskName);
    }
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
        // MyRTOS_Port_Yield();
    }
    return 0;
}

/**
 * @brief 将当前任务延迟指定的滴答数
 * @param tick 要延迟的系统滴答数
 */
void Task_Delay(uint32_t tick) {
    if (tick == 0 || g_scheduler_started == 0)
        return;
    MyRTOS_Port_EnterCritical(); {
        // 从就绪链表中移除当前任务
        removeTaskFromList(get_ready_task_list(currentTask->priority), currentTask);
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
 * @brief 挂起指定的任务.
 */
void Task_Suspend(TaskHandle_t task_h) {
    Task_t *task_to_suspend = (task_h == NULL) ? currentTask : task_h;
    // 不允许挂起空闲任务
    if (task_to_suspend == idleTask || task_to_suspend == NULL) {
        return;
    }
    MyRTOS_Port_EnterCritical();
    // 如果任务已经是挂起状态, 则不做任何事.
    if (task_to_suspend->state == TASK_STATE_SUSPENDED) {
        MyRTOS_Port_ExitCritical();
        return;
    }
    // 从其当前所在的列表中移除.
    if (task_to_suspend->state == TASK_STATE_READY) {
        removeTaskFromList(get_ready_task_list(task_to_suspend->priority), task_to_suspend);
    } else if (task_to_suspend->state == TASK_STATE_DELAYED || task_to_suspend->state == TASK_STATE_BLOCKED) {
        // 任务可能同时在延迟列表和事件列表中
        if(task_to_suspend->delay > 0) {
            removeTaskFromList(get_delayed_task_list_head(), task_to_suspend);
        }
        if(task_to_suspend->pEventList != NULL) {
            eventListRemove(task_to_suspend);
        }
    }
    // 设置状态为挂起.
    task_to_suspend->state = TASK_STATE_SUSPENDED;
    MyRTOS_Port_ExitCritical();
    // 如果挂起的是当前任务, 则需要触发一次调度来运行其他任务.
    if (task_to_suspend == currentTask) {
        MyRTOS_Port_Yield();
    }
}

/**
 * @brief 恢复一个被挂起的任务.
 */
void Task_Resume(TaskHandle_t task_h) {
    Task_t *task_to_resume = (Task_t *)task_h;

    if (task_to_resume == NULL) {
        return;
    }

    int trigger_yield = 0;
    MyRTOS_Port_EnterCritical();

    // 仅当任务确实处于挂起状态时才恢复.
    if (task_to_resume->state == TASK_STATE_SUSPENDED) {
        // 将任务重新添加到就绪队列.
        addTaskToReadyList(task_to_resume);

        // 检查是否需要进行上下文切换.
        if (task_to_resume->priority > currentTask->priority) {
            trigger_yield = 1;
        }
    }

    MyRTOS_Port_ExitCritical();

    if (trigger_yield) {
        MyRTOS_Port_Yield();
    }
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
    if (higherPriorityTaskWoken == NULL)
        return -1;
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
        removeTaskFromList(get_ready_task_list(currentTask->priority), currentTask);
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
TaskState_t Task_GetState(TaskHandle_t task_h) { return task_h ? ((Task_t *) task_h)->state : TASK_STATE_UNUSED; }

/**
 * @brief 获取任务的当前优先级
 * @param task_h 目标任务的句柄
 * @return 返回任务的优先级
 */
uint8_t Task_GetPriority(TaskHandle_t task_h) { return task_h ? ((Task_t *) task_h)->priority : 0; }

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
    if (!task_h)
        return (uint32_t) -1;
    return ((Task_t *) task_h)->taskId;
}

/**
 * @brief 获取任务的名称。
 * @param task_h 要查询的任务句柄。
 * @return 返回指向任务名称字符串的指针。如果句柄无效，可能返回NULL或空字符串。
 */
char *Task_GetName(TaskHandle_t task_h) {
    // 进行一个基本的句柄有效性检查
    if (task_h == NULL) {
        return ((Task_t *) currentTask)->taskName;
    }
    return ((Task_t *) task_h)->taskName;
}

/**
 * @brief 根据任务名称查找任务句柄。
 * @param taskName 要查找的任务的名称字符串。
 * @return 如果找到，则返回任务的句柄；如果未找到，则返回 NULL。
 */
TaskHandle_t Task_FindByName(const char *taskName) {
    TaskHandle_t found_task = NULL;

    if (taskName == NULL) {
        return NULL;
    }
    // 进入临界区以安全地遍历全局任务列表
    MyRTOS_Port_EnterCritical();
    Task_t *p_iterator = allTaskListHead;
    while (p_iterator != NULL) {
        // 使用 strcmp 比较任务名称
        if (strcmp(p_iterator->taskName, taskName) == 0) {
            found_task = p_iterator;
            break; // 找到后立即退出循环
        }
        p_iterator = p_iterator->pNextTask;
    }
    MyRTOS_Port_ExitCritical();
    return found_task;
}
