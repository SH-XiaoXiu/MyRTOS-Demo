/**
 * @file myrtos_kernel.h
 * @brief MyRTOS 内核内部共享头文件
 * @note  此头文件仅供内核模块内部使用，不对外暴露
 */

#ifndef MYRTOS_KERNEL_H
#define MYRTOS_KERNEL_H

#include "MyRTOS.h"
#include "MyRTOS_Kernel_Private.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_Extension.h"

#include <stdio.h>
#include <string.h>

/*===========================================================================*
 * 内部宏定义
 *===========================================================================*/

// 内存堆中允许的最小内存块大小，至少能容纳两个BlockLink_t结构体
#define HEAP_MINIMUM_BLOCK_SIZE ((sizeof(BlockLink_t) * 2))

/*===========================================================================*
 * 内核全局变量声明 (extern)
 *===========================================================================*/

// 内存管理
extern size_t freeBytesRemaining;

// 内核状态
extern volatile uint8_t g_scheduler_started;
extern volatile uint32_t criticalNestingCount;
extern TaskHandle_t allTaskListHead;
extern TaskHandle_t currentTask;
extern TaskHandle_t idleTask;

/*===========================================================================*
 * 内部函数声明
 *===========================================================================*/

// 调度器相关
void addTaskToReadyList(TaskHandle_t task);
void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t taskToRemove);
void addTaskToSortedDelayList(TaskHandle_t task);

// 事件列表相关
void eventListInit(EventList_t *pEventList);
void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert);
void eventListRemove(TaskHandle_t taskToRemove);

// 信号相关
int check_signal_wait_condition(TaskHandle_t task);

// 扩展机制
void broadcast_event(const KernelEventData_t *pEventData);

// 调度器内部
void scheduler_init(void);
TaskHandle_t *get_delayed_task_list_head(void);
TaskHandle_t *get_ready_task_list(uint8_t priority);
void task_set_priority(TaskHandle_t task, uint8_t newPriority);

#endif /* MYRTOS_KERNEL_H */
