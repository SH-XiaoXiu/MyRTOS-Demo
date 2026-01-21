/**
 * @file myrtos_semaphore.c
 * @brief MyRTOS 信号量模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 外部函数声明
 *===========================================================================*/
extern TaskHandle_t *get_delayed_task_list_head(void);
extern TaskHandle_t *get_ready_task_list(uint8_t priority);

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

/**
 * @brief 创建一个计数信号量
 * @param maxCount 信号量的最大计数值
 * @param initialCount 信号量的初始计数值
 * @return 成功则返回信号量句柄，失败则返回NULL
 */
SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount) {
    if (maxCount == 0 || initialCount > maxCount)
        return NULL;
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
    if (semaphore == NULL)
        return;
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
    if (semaphore == NULL)
        return 0;
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
        removeTaskFromList(get_ready_task_list(currentTask->priority), currentTask);
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
        if (currentTask->pEventList == NULL)
            return 1;
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
    if (semaphore == NULL)
        return 0;
    int trigger_yield = 0;
    MyRTOS_Port_EnterCritical();
    // 如果有任务在等待信号量，则直接唤醒一个，而不增加计数值
    if (semaphore->eventList.head != NULL) {
        Task_t *taskToWake = semaphore->eventList.head;
        eventListRemove(taskToWake);
        if (taskToWake->delay > 0) {
            removeTaskFromList(get_delayed_task_list_head(), taskToWake);
            taskToWake->delay = 0;
        }
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority)
            trigger_yield = 1;
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
    if (semaphore == NULL || pxHigherPriorityTaskWoken == NULL)
        return 0;
    *pxHigherPriorityTaskWoken = 0;
    int result = 0;
    MyRTOS_Port_EnterCritical();
    // 逻辑与 Semaphore_Give 类似
    if (semaphore->eventList.head != NULL) {
        Task_t *taskToWake = semaphore->eventList.head;
        eventListRemove(taskToWake);
        if (taskToWake->delay > 0) {
            removeTaskFromList(get_delayed_task_list_head(), taskToWake);
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
