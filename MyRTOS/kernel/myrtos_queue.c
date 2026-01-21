/**
 * @file myrtos_queue.c
 * @brief MyRTOS 消息队列模块
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
 * @brief 创建一个消息队列
 * @param length 队列能够存储的最大项目数
 * @param itemSize 每个项目的大小（字节）
 * @return 成功则返回队列句柄，失败则返回NULL
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize) {
    if (length == 0 || itemSize == 0)
        return NULL;
    // 分配队列控制结构内存
    Queue_t *queue = MyRTOS_Malloc(sizeof(Queue_t));
    if (queue == NULL)
        return NULL;
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
    if (queue == NULL)
        return;
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
    if (pQueue == NULL)
        return 0;
    while (1) {
        MyRTOS_Port_EnterCritical();
        // 情况1: 有任务正在等待接收数据
        if (pQueue->receiveEventList.head != NULL) {
            Task_t *taskToWake = pQueue->receiveEventList.head;
            eventListRemove(taskToWake);
            // 如果该任务同时也在延迟列表中，从中移除
            if (taskToWake->delay > 0) {
                removeTaskFromList(get_delayed_task_list_head(), taskToWake);
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
        removeTaskFromList(get_ready_task_list(currentTask->priority), currentTask);
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
        if (currentTask->pEventList == NULL)
            continue; // 如果是正常唤醒，pEventList会被设为NULL，循环重试发送
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
    if (pQueue == NULL)
        return 0;
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
                    removeTaskFromList(get_delayed_task_list_head(), taskToWake);
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
        removeTaskFromList(get_ready_task_list(currentTask->priority), currentTask);
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
        if (currentTask->pEventList == NULL)
            return 1;
        // 如果是超时唤醒
        MyRTOS_Port_EnterCritical();
        eventListRemove(currentTask);
        currentTask->eventData = NULL;
        MyRTOS_Port_ExitCritical();
        return 0;
    }
}
