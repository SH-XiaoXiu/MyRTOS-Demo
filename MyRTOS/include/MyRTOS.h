#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>
#include "MyRTOS_Config.h" // 用户的配置文件将包含所有功能开关

// -----------------------------
// 时间转换宏
// -----------------------------
// 毫秒转tick
#define MS_TO_TICKS(ms)         (((uint64_t)(ms) * MY_RTOS_TICK_RATE_HZ) / 1000)
// tick转毫秒
#define TICK_TO_MS(tick)        (((uint64_t)(tick) * 1000) / MY_RTOS_TICK_RATE_HZ)

// -----------------------------
// 前置声明
// -----------------------------
struct Task_t;
struct Mutex_t;
struct Timer_t;
struct Queue_t;

// -----------------------------
// 任务状态枚举
// -----------------------------
typedef enum {
    TASK_STATE_UNUSED = 0, // 任务控制块未使用
    TASK_STATE_READY,      // 任务就绪
    TASK_STATE_DELAYED,    // 任务延时中
    TASK_STATE_BLOCKED     // 任务因等待事件阻塞
} TaskState_t;

// -----------------------------
// 核心对象句柄
// -----------------------------
typedef struct Task_t *TaskHandle_t;
typedef struct Mutex_t *MutexHandle_t;
typedef struct Timer_t *TimerHandle_t;
typedef void *QueueHandle_t;
struct Semaphore_t;
typedef struct Semaphore_t *SemaphoreHandle_t;

// -----------------------------
// 定时器回调函数指针类型
// -----------------------------
typedef void (*TimerCallback_t)(TimerHandle_t timer);

// =============================
// System Core
// =============================
/**
 * @brief 统一的系统初始化函数。
 *        用户在 main 中应首先调用此函数。
 */
void MyRTOS_SystemInit(void);

/**
 * @brief 初始化RTOS内核的核心数据结构。
 *        此函数由 MyRTOS_SystemInit 自动调用。
 */
void MyRTOS_Init(void);

/**
 * @brief 启动调度器，开始任务调度
 */
void Task_StartScheduler(void);

/**
 * @brief 获取系统tick计数
 * @return 当前系统tick
 */
uint64_t MyRTOS_GetTick(void);

/**
 * @brief 内核的Tick处理函数，由移植层的Tick中断调用
 */
void MyRTOS_Tick_Handler(void);

// =============================
// Task Management
// =============================
/**
 * @brief 创建任务
 * @param func 任务函数指针
 * @param taskName 任务名称，可用于调试或统计
 * @param stack_size 栈大小(单位：word, 4 bytes)
 * @param param 任务参数
 * @param priority 任务优先级
 * @return 任务句柄
 */
TaskHandle_t Task_Create(void (*func)(void *),
                         const char *taskName,
                         uint16_t stack_size,
                         void *param,
                         uint8_t priority);

/**
 * @brief 删除任务
 * @param task_h 任务句柄，NULL表示删除当前任务
 * @return 0成功，-1失败
 */
int Task_Delete(TaskHandle_t task_h);

/**
 * @brief 延时任务
 * @param tick 延时时间(tick)
 */
void Task_Delay(uint32_t tick);

/**
 * @brief 发送通知给任务
 * @param task_h 任务句柄
 * @return 0成功，-1失败
 */
int Task_Notify(TaskHandle_t task_h);


/**
 * @brief [ISR安全] 发送通知给任务
 * @param task_h 任务句柄
 * @param higherPriorityTaskWoken 如果唤醒的任务优先级更高，此指针指向的值将被设为1
 * @return 0成功，-1失败
 */
int Task_NotifyFromISR(TaskHandle_t task_h, int *higherPriorityTaskWoken);

/**
 * @brief 等待任务通知
 */
void Task_Wait(void);

/**
 * @brief 获取任务状态
 * @param task_h 任务句柄
 * @return 任务状态(TaskState_t)
 */
TaskState_t Task_GetState(TaskHandle_t task_h);

/**
 * @brief 获取任务优先级
 * @param task_h 任务句柄
 * @return 优先级
 */
uint8_t Task_GetPriority(TaskHandle_t task_h);

/**
 * @brief 获取当前运行的任务句柄
 * @return 当前任务句柄
 */
TaskHandle_t Task_GetCurrentTaskHandle(void);

// =============================
// Queue Management
// =============================
/**
 * @brief 创建队列
 * @param length 队列长度
 * @param itemSize 队列元素大小
 * @return 队列句柄
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

/**
 * @brief 删除队列
 * @param delQueue 队列句柄
 */
void Queue_Delete(QueueHandle_t delQueue);

/**
 * @brief 向队列发送数据
 * @param queue 队列句柄
 * @param item 待发送数据
 * @param block_ticks 阻塞时间，0不阻塞，MY_RTOS_MAX_DELAY无限等待
 * @return 1成功，0失败
 */
int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks);

/**
 * @brief 从队列接收数据
 * @param queue 队列句柄
 * @param buffer 接收缓冲区
 * @param block_ticks 阻塞时间，0不阻塞，MY_RTOS_MAX_DELAY无限等待
 * @return 1成功，0失败
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks);

// =============================
// Timer Management (Software Timers)
// =============================
/**
 * @brief 创建定时器
 * @param delay 启动延时(tick)
 * @param period 定时周期(tick)，0表示一次性定时器
 * @param callback 回调函数
 * @param arg 回调函数参数
 * @return 定时器句柄
 */
TimerHandle_t Timer_Create(uint32_t delay, uint32_t period, TimerCallback_t callback, void *arg);

/**
 * @brief 启动定时器
 * @param timer 定时器句柄
 * @return 0成功，-1失败
 */
int Timer_Start(TimerHandle_t timer);

/**
 * @brief 停止定时器
 * @param timer 定时器句柄
 * @return 0成功，-1失败
 */
int Timer_Stop(TimerHandle_t timer);

/**
 * @brief 删除定时器
 * @param timer 定时器句柄
 * @return 0成功，-1失败
 */
int Timer_Delete(TimerHandle_t timer);

// =============================
// Mutex Management
// =============================
/**
 * @brief 创建互斥锁
 * @return 成功返回句柄，失败返回NULL
 */
MutexHandle_t Mutex_Create(void);

/**
 * @brief 获取互斥锁
 */
void Mutex_Lock(MutexHandle_t mutex);

/**
 * @brief 带超时的锁
 * @param block_ticks 阻塞tick数，0表示不阻塞
 * @return 1成功，0超时失败
 */
int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks);

/**
 * @brief 释放互斥锁
 */
void Mutex_Unlock(MutexHandle_t mutex);

/**
 * @brief 递归锁获取
 */
void Mutex_Lock_Recursive(MutexHandle_t mutex);

/**
 * @brief 递归锁释放
 */
void Mutex_Unlock_Recursive(MutexHandle_t mutex);




// =============================
// Semaphore Management
// =============================
/**
 * @brief 创建一个计数信号量
 * @param maxCount 信号量能达到的最大计数值
 * @param initialCount 信号量的初始计数值
 * @return 成功返回信号量句柄，失败返回NULL
 */
SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount);

/**
 * @brief 删除一个信号量
 * @param semaphore 要删除的信号量句柄
 */
void Semaphore_Delete(SemaphoreHandle_t semaphore);

/**
 * @brief 获取(P操作)一个信号量
 * @param semaphore 信号量句柄
 * @param block_ticks 阻塞时间(ticks)。0表示不阻塞，MY_RTOS_MAX_DELAY表示无限等待
 * @return 1表示成功获取，0表示超时失败
 */
int Semaphore_Take(SemaphoreHandle_t semaphore, uint32_t block_ticks);

/**
 * @brief 释放(V操作)一个信号量
 * @param semaphore 信号量句柄
 * @return 1表示成功释放，0表示信号量已达最大值
 */
int Semaphore_Give(SemaphoreHandle_t semaphore);


/**
 * @brief [ISR安全] 释放(V操作)一个信号量
 * @param semaphore 信号量句柄
 * @param higherPriorityTaskWoken 如果唤醒的任务优先级更高，此指针指向的值将被设为1
 * @return 1表示成功释放，0表示信号量已达最大值
 */
int Semaphore_GiveFromISR(SemaphoreHandle_t semaphore, int *higherPriorityTaskWoken);

// =============================
// 运行时统计 API
// =============================
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
/**
 * @brief 任务运行统计信息
 */
typedef struct TaskStats_t {
    uint32_t taskId;                      // 任务ID
#if (MY_RTOS_TASK_NAME_MAX_LEN > 0)
    char taskName[MY_RTOS_TASK_NAME_MAX_LEN]; // 任务名称
#endif
    TaskState_t state;                    // 当前任务状态
    uint8_t currentPriority;              // 当前优先级
    uint8_t basePriority;                 // 基础优先级
    uint64_t runTimeCounter;              // 累计运行时间(单位:由高精度定时器频率决定)
    uint32_t stackHighWaterMark;          // 栈水位(已用栈大小, bytes)
    uint32_t stackSize;                   // 栈大小(bytes)
} TaskStats_t;

/**
 * @brief 堆内存统计信息
 */
typedef struct HeapStats_t {
    size_t totalHeapSize;                 // 堆总大小
    size_t freeBytesRemaining;            // 当前剩余内存
    size_t minimumEverFreeBytesRemaining; // 历史最小剩余内存
} HeapStats_t;

/**
 * @brief 获取任务统计信息
 */
void Task_GetInfo(TaskHandle_t taskHandle, TaskStats_t *pTaskStats);

/**
 * @brief 获取下一个任务句柄，用于迭代任务
 */
TaskHandle_t Task_GetNextTaskHandle(TaskHandle_t lastTaskHandle);

/**
 * @brief 获取堆统计信息
 */
void Heap_GetStats(HeapStats_t *pHeapStats);

#endif // MY_RTOS_GENERATE_RUN_TIME_STATS

#endif // MYRTOS_H