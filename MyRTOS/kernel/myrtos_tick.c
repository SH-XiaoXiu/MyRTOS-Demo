/**
 * @file myrtos_tick.c
 * @brief MyRTOS 系统滴答处理模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 外部函数声明
 *===========================================================================*/
extern TaskHandle_t *get_delayed_task_list_head(void);

/*===========================================================================*
 * 私有变量
 *===========================================================================*/

// 系统滴答计数器
static volatile uint64_t systemTickCount = 0;

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

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
    TaskHandle_t *pDelayedListHead = get_delayed_task_list_head();

    // 增加系统滴答计数
    systemTickCount++;
    const uint64_t current_tick = systemTickCount;
    // 检查延迟链表头，看是否有任务的唤醒时间已到
    while (*pDelayedListHead != NULL && (*pDelayedListHead)->delay <= current_tick) {
        Task_t *taskToWake = *pDelayedListHead;
        // 从延迟链表中移除
        removeTaskFromList(pDelayedListHead, taskToWake);
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
uint8_t MyRTOS_Schedule_IsRunning(void) { return g_scheduler_started; }
