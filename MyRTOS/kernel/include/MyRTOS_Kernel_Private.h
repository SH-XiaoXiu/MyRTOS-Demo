//
// Created by XiaoXiu on 8/30/2025.
//

#ifndef MYRTOS_KERNEL_PRIVATE_H
#define MYRTOS_KERNEL_PRIVATE_H

#include "MyRTOS.h"
#include "MyRTOS_Port.h"

/*===========================================================================*
 *                      内部数据结构定义                                     *
 *===========================================================================*/

// --- 内存管理结构 ---
/**
 * @brief 内存块链接结构体
 */
typedef struct BlockLink_t {
    struct BlockLink_t *nextFreeBlock; // 指向下一个空闲内存块
    size_t blockSize; // 当前内存块大小
} BlockLink_t;


// --- 内核核心对象结构 ---
/**
 * @brief 事件列表结构体
 */
typedef struct EventList_t {
    volatile TaskHandle_t head; // 事件列表头节点
} EventList_t;

/**
 * @brief 互斥锁结构体
 */
typedef struct Mutex_t {
    volatile int locked; // 锁状态标记
    struct Task_t *owner_tcb; // 拥有该互斥锁的任务TCB
    struct Mutex_t *next_held_mutex; // 下一个持有的互斥锁
    EventList_t eventList; // 等待该互斥锁的任务事件列表
    volatile uint32_t recursion_count; // 递归锁定计数
} Mutex_t;

/**
 * @brief 任务控制块结构体
 */
typedef struct Task_t {
    StackType_t *sp; // 任务栈指针

    void (*func)(void *); // 任务函数指针

    void *param; // 任务函数参数
    uint64_t delay; // 任务延时计数
    volatile uint32_t notification; // 任务通知值
    volatile uint8_t is_waiting_notification; // 是否正在等待通知
    volatile TaskState_t state; // 任务状态
    uint32_t taskId; // 任务ID
    StackType_t *stack_base; // 任务栈基地址
    uint8_t priority; // 任务优先级
    uint8_t basePriority; // 任务基础优先级
    struct Task_t *pNextTask; // 指向下一个任务(就绪链表)
    struct Task_t *pNextGeneric; // 通用链表下一节点指针
    struct Task_t *pPrevGeneric; // 通用链表上一节点指针
    struct Task_t *pNextEvent; // 事件链表下一节点指针
    EventList_t *pEventList; // 任务所属事件列表
    Mutex_t *held_mutexes_head; // 任务持有的互斥锁链表头
    void *eventData; // 事件相关数据
    const char *taskName; // 任务名称
    uint16_t stackSize_words; // 任务栈大小(字)
} Task_t;

// TCB中stack_base字段的偏移量
#define TCB_OFFSET_STACK_BASE   offsetof(Task_t, stack_base)

/**
 * @brief 队列结构体
 */
typedef struct Queue_t {
    uint8_t *storage; // 队列存储区域
    uint32_t length; // 队列长度
    uint32_t itemSize; // 队列项大小
    volatile uint32_t waitingCount; // 等待计数
    uint8_t *writePtr; // 写指针
    uint8_t *readPtr; // 读指针
    EventList_t sendEventList; // 发送事件列表
    EventList_t receiveEventList; // 接收事件列表
} Queue_t;

/**
 * @brief 信号量结构体
 */
typedef struct Semaphore_t {
    volatile uint32_t count; // 信号量计数
    uint32_t maxCount; // 信号量最大计数
    EventList_t eventList; // 等待该信号量的任务事件列表
} Semaphore_t;


/*===========================================================================*
 *                      内部全局变量声明                                     *
 *===========================================================================*/
extern TaskHandle_t allTaskListHead; // 所有任务链表头
extern size_t freeBytesRemaining; // 剩余空闲内存字节数

#endif // MYRTOS_KERNEL_PRIVATE_H


