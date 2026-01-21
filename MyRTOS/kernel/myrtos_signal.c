/**
 * @file myrtos_signal.c
 * @brief MyRTOS 信号机制模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 外部函数声明
 *===========================================================================*/
extern TaskHandle_t *get_delayed_task_list_head(void);
extern TaskHandle_t *get_ready_task_list(uint8_t priority);

/*===========================================================================*
 * 内部函数实现
 *===========================================================================*/

int check_signal_wait_condition(TaskHandle_t task) {
    if (task == NULL) return 0;

    const uint32_t pending = task->signals_pending;
    const uint32_t mask = task->signals_wait_mask;
    const uint32_t options = task->wait_options;

    if (options & SIGNAL_WAIT_ALL) {
        return (pending & mask) == mask;
    } else {
        // 默认为 SIGNAL_WAIT_ANY
        return (pending & mask) != 0;
    }
}

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

int Task_SendSignal(TaskHandle_t target_task, uint32_t signals) {
    if (target_task == NULL || signals == 0) {
        return -1;
    }

    Task_t *pTargetTask = (Task_t *) target_task;
    int trigger_yield = 0;

    MyRTOS_Port_EnterCritical(); {
        // 原子地设置目标任务的待处理信号
        pTargetTask->signals_pending |= signals;

        //检查目标任务是否正在等待信号 (通过检查其pEventList是否指向自己的signal_event_list)
        if (pTargetTask->pEventList == &pTargetTask->signal_event_list) {
            //检查唤醒条件是否满足
            if (check_signal_wait_condition(pTargetTask)) {
                //唤醒任务
                //从它自己的事件列表中移除 (这会将其pEventList设为NULL)
                eventListRemove(pTargetTask);
                // 如果任务因超时也存在于延迟列表中，则一并移除
                if (pTargetTask->delay > 0) {
                    removeTaskFromList(get_delayed_task_list_head(), pTargetTask);
                    pTargetTask->delay = 0;
                }
                addTaskToReadyList(pTargetTask);
                // 检查是否需要调度
                if (pTargetTask->priority > currentTask->priority) {
                    trigger_yield = 1;
                }
            }
        }
    }
    MyRTOS_Port_ExitCritical();

    if (trigger_yield) {
        MyRTOS_Port_Yield();
    }
    return 0;
}


int Task_SendSignalFromISR(TaskHandle_t target_task, uint32_t signals, int *higherPriorityTaskWoken) {
    if (target_task == NULL || signals == 0 || higherPriorityTaskWoken == NULL) {
        return -1;
    }

    Task_t *pTargetTask = (Task_t *) target_task;
    *higherPriorityTaskWoken = 0;

    MyRTOS_Port_EnterCritical(); {
        pTargetTask->signals_pending |= signals;

        if (pTargetTask->pEventList == &pTargetTask->signal_event_list) {
            if (check_signal_wait_condition(pTargetTask)) {
                eventListRemove(pTargetTask);
                if (pTargetTask->delay > 0) {
                    removeTaskFromList(get_delayed_task_list_head(), pTargetTask);
                    pTargetTask->delay = 0;
                }
                addTaskToReadyList(pTargetTask);

                if (pTargetTask->priority > currentTask->priority) {
                    *higherPriorityTaskWoken = 1;
                }
            }
        }
    }
    MyRTOS_Port_ExitCritical();

    return 0;
}


uint32_t Task_WaitSignal(uint32_t signal_mask, uint32_t block_ticks, uint32_t options) {
    if (signal_mask == 0) {
        return 0; // 等待一个空掩码没有意义
    }

    uint32_t received_signals = 0;

    MyRTOS_Port_EnterCritical(); {
        //检查信号是否已经存在，无需阻塞
        uint32_t pending = currentTask->signals_pending;
        int condition_met = 0;
        if (options & SIGNAL_WAIT_ALL) {
            condition_met = (pending & signal_mask) == signal_mask;
        } else {
            condition_met = (pending & signal_mask) != 0;
        }

        if (condition_met) {
            // 信号已满足，直接处理并返回
            received_signals = pending;
            if (options & SIGNAL_CLEAR_ON_EXIT) {
                // 清除那些满足了等待条件的信号
                currentTask->signals_pending &= ~(pending & signal_mask);
            }
            MyRTOS_Port_ExitCritical();
            return received_signals;
        }

        //信号不满足，需要阻塞
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0; // 不阻塞，直接返回0表示超时
        }

        //配置任务的等待状态
        currentTask->signals_wait_mask = signal_mask;
        currentTask->wait_options = options;

        //将任务从就绪列表移除，并加入它自己的事件列表
        removeTaskFromList(get_ready_task_list(currentTask->priority), currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&currentTask->signal_event_list, currentTask);

        //处理超时
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
    }
    MyRTOS_Port_ExitCritical();

    MyRTOS_Port_Yield();

    MyRTOS_Port_EnterCritical(); {
        //判断是正常唤醒还是超时
        // 如果pEventList不为NULL，说明任务是因超时被TickHandler移到就绪队列的，
        // 而不是被Task_SendSignal正常唤醒（SendSignal会调用eventListRemove将pEventList清为NULL）。
        if (currentTask->pEventList != NULL) {
            // 超时了，手动从事件列表中移除自己
            eventListRemove(currentTask);
            received_signals = 0; // 返回0表示超时
        } else {
            // 被正常唤醒
            received_signals = currentTask->signals_pending;
            if (options & SIGNAL_CLEAR_ON_EXIT) {
                currentTask->signals_pending &= ~(received_signals & signal_mask);
            }
        }

        //清理等待状态
        currentTask->signals_wait_mask = 0;
        currentTask->wait_options = 0;
    }
    MyRTOS_Port_ExitCritical();

    return received_signals;
}


int Task_ClearSignal(TaskHandle_t task_to_clear, uint32_t signals_to_clear) {
    Task_t *pTask = (task_to_clear == NULL) ? currentTask : (Task_t *) task_to_clear;

    if (pTask == NULL) {
        return -1;
    }

    MyRTOS_Port_EnterCritical(); {
        pTask->signals_pending &= ~signals_to_clear;
    }
    MyRTOS_Port_ExitCritical();

    return 0;
}
