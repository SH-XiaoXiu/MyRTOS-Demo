#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>

#define MAX_TASKS   8
#define STACK_SIZE  256
#define IDLE_TASK_ID (MAX_TASKS-1)
#define MY_RTOS_MAX_PRIORITIES    (16)

// 调试输出开关
#define DEBUG_PRINT 1  // 设置为1开启调试输出,0关闭
//调试输出函数
#if DEBUG_PRINT
#include <stdio.h>
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

// 定义任务状态
typedef enum {
    TASK_STATE_UNUSED = 0, // 未使用
    TASK_STATE_READY, //就绪，可以运行
    TASK_STATE_DELAYED, //正在延时
    TASK_STATE_BLOCKED // 任务因等待资源（如互斥锁、通知）而阻塞
} TaskState_t;


struct Task_t;

// 互斥锁结构体
typedef struct Mutex_t {
    volatile int locked;
    volatile uint32_t owner;
    volatile uint32_t waiting_mask;
    struct Task_t *owner_tcb;
    struct Mutex_t *next_held_mutex;
} Mutex_t;

// 任务控制块 (TCB)
//为了保持汇编代码的兼容性 sp 必须是结构体的第一个成员
typedef struct Task_t {
    uint32_t *sp;
    void (*func)(void *);     // 任务函数
    void *param;              // 任务参数
    uint32_t delay;           // 延时
    volatile uint32_t notification;
    volatile uint8_t is_waiting_notification;
    volatile TaskState_t state; // 任务状态
    uint32_t taskId;          // 任务ID
    uint32_t *stack_base;     // 栈基地址,用于free
    uint8_t priority;         //任务优先级
    struct Task_t *pNextTask; //用于所有任务链表
    struct Task_t *pNextReady; //用于就绪或延时链表
    struct Task_t *pPrevReady; //用于双向链表,方便删除 O(1)复杂度
    Mutex_t *held_mutexes_head;
    void *eventObject;         // 指向正在等待的内核对象
    void *eventData;           // 用于传递与事件相关的数据指针 (如消息的源/目的地址)
    struct Task_t *pNextEvent;  // 用于构建内核对象的等待任务链表
} Task_t;


typedef void* QueueHandle_t;

typedef struct Queue_t {
    uint8_t *storage;           // 指向队列存储区的指针
    uint32_t length;            // 队列最大能容纳的消息数
    uint32_t itemSize;          // 每个消息的大小
    volatile uint32_t waitingCount; // 当前队列中的消息数
    uint8_t *writePtr;          // 下一个要写入数据的位置
    uint8_t *readPtr;           // 下一个要读取数据的位置
    // 等待列表 (将按任务优先级排序)
    Task_t *sendWaitList;
    Task_t *receiveWaitList;
} Queue_t;

/**
 * @brief 创建一个消息队列
 * @param length 队列能够容纳的最大消息数量
 * @param itemSize 每个消息的大小 (字节)
 * @return 成功返回队列句柄，失败返回 NULL
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

/**
 * @brief 删除一个消息队列
 * @param delQueue 要删除的队列句柄
 */
void Queue_Delete(QueueHandle_t delQueue);

/**
 * @brief 向队列发送一个消息
 * @param queue 队列句柄
 * @param item 指向要发送的消息的指针
 * @param block 0: 不阻塞, 1: 永久阻塞直到发送成功
 * @return 1 表示成功, 0 表示失败 (队列满且不阻塞)
 */
int Queue_Send(QueueHandle_t queue, const void *item, int block);

/**
 * @brief 从队列接收一个消息
 * @param queue 队列句柄
 * @param buffer 用于存放接收到的消息的缓冲区指针
 * @param block 0: 不阻塞, 1: 永久阻塞直到接收到消息
 * @return 1 表示成功, 0 表示失败 (队列空且不阻塞)
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, int block);


/**
 * @brief 进入临界区
 *
 * 该宏会保存当前的中断状态（PRIMASK寄存器），然后禁用所有可屏蔽中断。
 * 必须与 MY_RTOS_EXIT_CRITICAL 成对使用。
 *
 * @param status_var 一个 uint32_t 类型的局部变量，用于保存中断状态。
 */
#define MY_RTOS_ENTER_CRITICAL(status_var)   \
do {                                     \
(status_var) = __get_PRIMASK();      \
__disable_irq();                     \
} while(0)

/**
 * @brief 退出临界区
 *
 * 该宏会恢复由 MY_RTOS_ENTER_CRITICAL 保存的中断状态。
 *
 * @param status_var 之前用于保存中断状态的同一个变量。
 */
#define MY_RTOS_EXIT_CRITICAL(status_var)       \
do {                                            \
__set_PRIMASK(status_var);                      \
} while(0)


/**
 * @brief 手动触发任务调度
 *
 * 该宏会挂起 PendSV 中断，请求调度器在适当的时候（通常是所有其他中断处理完毕后）
 * 进行一次任务上下文切换, j就是任务让步
 */
#define MY_RTOS_YIELD()                      \
do {                                     \
SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; \
__ISB();                             \
} while(0)

void MyRTOS_Init(void);

Task_t *Task_Create(void (*func)(void *), void *param, uint8_t priority) ;

int Task_Delete(const Task_t *task_h);

void Task_StartScheduler(void);

void Task_Delay(uint32_t tick);

int Task_Notify(uint32_t task_id);

void Task_Wait(void);

void Mutex_Init(Mutex_t *mutex);

void Mutex_Lock(Mutex_t *mutex);

void Mutex_Unlock(Mutex_t *mutex);
#endif //MYRTOS_H
