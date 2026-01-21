/**
 * @file myrtos_mutex.c
 * @brief MyRTOS 互斥锁模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 外部函数声明
 *===========================================================================*/
extern void task_set_priority(TaskHandle_t task, uint8_t newPriority);
extern TaskHandle_t *get_delayed_task_list_head(void);
extern TaskHandle_t *get_ready_task_list(uint8_t priority);

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

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
 * @brief 删除一个互斥锁
 * @note  这是一个健壮的实现。它会安全地处理以下情况：
 *        1. 唤醒所有正在等待该锁的任务，防止它们永久阻塞。
 *        2. 如果锁当前被持有，会从持有者任务的内部链表中安全地移除该锁，防止悬空指针。
 *        应用程序应确保在删除锁之后，不再有任何代码尝试使用它。
 * @param mutex 要删除的互斥锁句柄
 */
void Mutex_Delete(MutexHandle_t mutex) {
    if (mutex == NULL) {
        return;
    }

    MyRTOS_Port_EnterCritical(); {
        // 唤醒所有正在等待该锁的任务
        //  遍历事件列表，将所有等待的任务移回到就绪列表
        while (mutex->eventList.head != NULL) {
            Task_t *taskToWake = mutex->eventList.head;
            // 从事件列表中移除任务。eventListRemove 会处理好 taskToWake->pEventList 的清理
            eventListRemove(taskToWake);
            // 如果任务因为超时也存在于延迟列表中，则一并移除
            if (taskToWake->delay > 0) {
                removeTaskFromList(get_delayed_task_list_head(), taskToWake);
                taskToWake->delay = 0;
            }
            // 将被唤醒的任务重新添加到就绪列表中
            addTaskToReadyList(taskToWake);
        }
        // 如果锁当前被持有，则从持有者任务中断开链接
        TaskHandle_t owner_tcb = mutex->owner_tcb;
        if (owner_tcb != NULL) {
            // 在任务持有的互斥锁链表中查找并移除此锁，以防止悬空指针
            // 要删除的锁是链表头
            if (owner_tcb->held_mutexes_head == mutex) {
                owner_tcb->held_mutexes_head = mutex->next_held_mutex;
            }
            // 要删除的锁在链表中间或末尾
            else {
                Mutex_t *p_iterator = owner_tcb->held_mutexes_head;
                // 遍历链表找到要删除锁的前一个节点
                while (p_iterator != NULL && p_iterator->next_held_mutex != mutex) {
                    p_iterator = p_iterator->next_held_mutex;
                }
                // 如果找到了前一个节点，就跳过要删除的锁
                if (p_iterator != NULL) {
                    p_iterator->next_held_mutex = mutex->next_held_mutex;
                }
            }
        }

        // 释放互斥锁结构本身占用的内存
        // 此时，已经没有任何任务的TCB或事件列表引用这个互斥锁了，可以安全释放
        MyRTOS_Free(mutex);
    }
    MyRTOS_Port_ExitCritical();
    // 被唤醒的任务会在下一次调度点（如时钟滴答）运行时获得CPU。
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
        removeTaskFromList(get_ready_task_list(currentTask->priority), currentTask);
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
        if (mutex->owner_tcb == currentTask)
            return 1; // 检查是否已成为新的持有者
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
void Mutex_Lock(MutexHandle_t mutex) { Mutex_Lock_Timeout(mutex, MYRTOS_MAX_DELAY); }

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
        while (p_iterator != NULL && p_iterator->next_held_mutex != mutex)
            p_iterator = p_iterator->next_held_mutex;
        if (p_iterator != NULL)
            p_iterator->next_held_mutex = mutex->next_held_mutex;
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
        if (taskToWake->priority > currentTask->priority)
            trigger_yield = 1;
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
    if (mutex->owner_tcb == currentTask)
        mutex->recursion_count = 1;
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
