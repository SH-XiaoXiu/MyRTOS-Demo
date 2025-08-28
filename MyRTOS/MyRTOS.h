#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>
#include "gd32f4xx.h" // 包含芯片相关的头文件以使用 CMSIS 核心函数

//RTOS Config
#define MY_RTOS_MAX_PRIORITIES    (16)      // 最大支持的优先级数量
#define MY_RTOS_TICK_RATE_HZ    (1000)    // 系统Tick频率 (Hz)
#define MY_RTOS_MAX_DELAY       (0xFFFFFFFFU) // 最大延时ticks

//Memory Management
#define RTOS_MEMORY_POOL_SIZE   (16 * 1024) // 内核使用的静态内存池大小 (bytes)
#define HEAP_BYTE_ALIGNMENT     (8)         // 内存对齐字节数

// 调试输出开关 (设置为1开启, 0关闭)
// #define DEBUG_PRINT 0

#if DEBUG_PRINT
#include <stdio.h>
#define DBG_PRINTF(...) do {                   \
     uint32_t primask;                         \
     MY_RTOS_ENTER_CRITICAL(primask);          \
     printf(__VA_ARGS__);                      \
     MY_RTOS_EXIT_CRITICAL(primask);           \
 } while (0)
#else
#define DBG_PRINTF(...)
#endif

// 时间转换宏
// 毫秒转ticks
#define MS_TO_TICKS(ms)         (((uint64_t)(ms) * MY_RTOS_TICK_RATE_HZ) / 1000)
// ticks转毫秒
#define TICK_TO_MS(tick)        (((uint64_t)(tick) * 1000) / MY_RTOS_TICK_RATE_HZ)

// 前置声明
struct Task_t;
struct Mutex_t;
struct Timer_t;
struct Queue_t;


// 任务状态枚举
typedef enum {
    TASK_STATE_UNUSED = 0, // 任务控制块未使用
    TASK_STATE_READY,      // 任务就绪，可以运行
    TASK_STATE_DELAYED,    // 任务正在延时
    TASK_STATE_BLOCKED     // 任务因等待事件(如互斥锁、队列)而阻塞
} TaskState_t;

// 核心对象句柄
typedef struct Task_t*      TaskHandle_t;
typedef struct Mutex_t*     MutexHandle_t;
typedef struct Timer_t*     TimerHandle_t;
typedef void*               QueueHandle_t;

// 定时器回调函数指针类型
typedef void (*TimerCallback_t)(TimerHandle_t timer);

// 核心宏
#define MY_RTOS_ENTER_CRITICAL(status_var)   do { (status_var) = __get_PRIMASK(); __disable_irq(); } while(0)
#define MY_RTOS_EXIT_CRITICAL(status_var)    do { __set_PRIMASK(status_var); } while(0)
#define MY_RTOS_YIELD()                      do { SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; __ISB(); } while(0)


/*----- System Core -----*/
/**
 * @brief 初始化系统核心
 */
void MyRTOS_Init(void);

/**
 * @brief 启动任务调度器
 */
void Task_StartScheduler(void);

/**
 * @brief 获取系统时间
 * @return 当前系统时间 (ticks)
 */
uint64_t MyRTOS_GetTick(void);

/*----- Task Management -----*/
/**
 * @brief 创建任务
 * @param func 任务函数
 * @param stack_size 任务栈大小 (words, e.g., 128 for 128*4 bytes)
 * @param param 任务参数
 * @param priority 任务优先级
 * @return 任务句柄
 */
TaskHandle_t Task_Create(void (*func)(void *), uint16_t stack_size, void *param, uint8_t priority);

/**
 * @brief 删除任务
 * @param task_h 任务句柄. 如果为 NULL, 则删除当前任务.
 * @return 0 成功，-1 失败
 */
int Task_Delete(TaskHandle_t task_h);

/**
 * @brief 延时
 * @param tick 延时时间 (ticks)
 */
void Task_Delay(uint32_t tick);

/**
 * @brief 通知任务
 * @param task_h 任务句柄
 * @return 0 成功，-1 失败
 */
int Task_Notify(TaskHandle_t task_h);

/**
 * @brief 等待任务通知
 */
void Task_Wait(void);


/**
 * @brief 获取指定任务的状态。
 * @param task_h 要查询的任务句柄。
 * @return 任务的当前状态 (TaskState_t)。
 */
TaskState_t Task_GetState(TaskHandle_t task_h);

/**
 * @brief 获取指定任务的优先级。
 * @param task_h 要查询的任务句柄。
 * @return 任务的优先级。
 */
uint8_t Task_GetPriority(TaskHandle_t task_h);

/**
 * @brief 获取当前正在运行的任务的句柄。
 * @return 当前任务的句柄。
 */
TaskHandle_t Task_GetCurrentTaskHandle(void);

/*----- Queue Management -----*/
/**
 * @brief 创建队列
 * @param length 队列长度
 * @param itemSize 数据项大小
 * @return 队列句柄
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

/**
 * @brief 删除队列
 * @param delQueue 要删除的队列句柄
 */
void Queue_Delete(QueueHandle_t delQueue);

/**
 * @brief 向队列发送数据
 * @param queue 队列句柄
 * @param item  要发送的数据项
 * @param block_ticks 阻塞时间 (ticks) 0表示不阻塞，MY_RTOS_MAX_DELAY表示无限等待
 * @return 1 表示成功, 0 表示失败
 */
int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks);

/**
 * @brief 从队列接收数据
 * @param queue 队列句柄
 * @param buffer 接收数据的缓冲区
 * @param block_ticks 阻塞时间 (ticks) 0表示不阻塞，MY_RTOS_MAX_DELAY表示无限等待
 * @return 1 表示成功, 0 表示失败
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks);

/*----- Timer Management -----*/
/**
 * @brief 创建定时器
 * @param delay 启动延时（ticks）
 * @param period 定时周期（ticks）0表示一次性定时器
 * @param callback 定时器回调函数
 * @param arg 回调函数参数
 * @return 定时器句柄
 */
TimerHandle_t Timer_Create(uint32_t delay, uint32_t period, TimerCallback_t callback, void* arg);

/**
 * @brief 启动定时器
 * @param timer 定时器句柄
 * @return 0 成功，-1 失败
 */
int Timer_Start(TimerHandle_t timer);

/**
 * @brief 停止定时器
 * @param timer 定时器句柄
 * @return 0 成功，-1 失败
 */
int Timer_Stop(TimerHandle_t timer);

/**
 * @brief 删除定时器
 * @param timer 定时器句柄
 * @return 0 成功，-1 失败
 */
int Timer_Delete(TimerHandle_t timer);

/*----- Mutex Management -----*/
/**
 * @brief 创建一个互斥锁.
 * @return 成功则返回互斥锁句柄, 失败(如内存不足)则返回 NULL.
 */
MutexHandle_t Mutex_Create(void);

/**
 * @brief 获取互斥锁
 * @param mutex 互斥锁句柄
 */
void Mutex_Lock(MutexHandle_t mutex);

/**
 * @brief 释放互斥锁
 * @param mutex 互斥锁句柄
 */
void Mutex_Unlock(MutexHandle_t mutex);


/**
 * @brief 递归获取互斥锁
 * @param mutex 互斥锁句柄
 */
void Mutex_Lock_Recursive(MutexHandle_t mutex);

/**
 * @brief 递归释放互斥锁
 * @param mutex 互斥锁句柄
 */
void Mutex_Unlock_Recursive(MutexHandle_t mutex);


#endif // MYRTOS_H