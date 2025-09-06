#ifndef MYRTOS_H
#define MYRTOS_H

#include <stddef.h>
#include <stdint.h>
#include "MyRTOS_Config.h"

// -----------------------------
// 时间转换宏
// -----------------------------
/**
 * @brief 将毫秒转换为系统时钟节拍数
 * @param ms 毫秒数
 * @return 对应的系统时钟节拍数
 */
#define MS_TO_TICKS(ms) (((uint64_t) (ms) * MYRTOS_TICK_RATE_HZ) / 1000)

/**
 * @brief 将系统时钟节拍数转换为毫秒
 * @param tick 系统时钟节拍数
 * @return 对应的毫秒数
 */
#define TICK_TO_MS(tick) (((uint64_t) (tick) * 1000) / MYRTOS_TICK_RATE_HZ)

// -----------------------------
// 信号宏
// -----------------------------
/**
 * @brief Task_WaitSignal 函数的选项宏
 * @details 这些宏可以通过按位或(|)操作组合使用
 */
#define SIGNAL_WAIT_ANY         (1U << 0) // 等待 signal_mask 中的任意一个信号
#define SIGNAL_WAIT_ALL         (1U << 1) // 等待 signal_mask 中的所有信号
#define SIGNAL_CLEAR_ON_EXIT    (1U << 2) // 从 Task_WaitSignal 返回时,自动清除满足条件的信号

// -----------------------------
// 前置声明 (Opaque Pointers)
// -----------------------------
struct Task_t;
struct Mutex_t;
struct Queue_t;
struct Semaphore_t;

// -----------------------------
// 任务状态枚举
// -----------------------------
/**
 * @brief 任务状态枚举类型
 */
typedef enum {
    TASK_STATE_UNUSED = 0, // 任务未使用
    TASK_STATE_READY, // 任务就绪状态
    TASK_STATE_DELAYED, // 任务延时状态
    TASK_STATE_BLOCKED // 任务阻塞状态
} TaskState_t;


/**
 * @brief 内核错误类型枚举
 */
typedef enum {
    KERNEL_ERROR_NONE = 0, // 无错误
    KERNEL_ERROR_STACK_OVERFLOW, // 栈溢出错误
    KERNEL_ERROR_MALLOC_FAILED, // 内存分配失败
    KERNEL_ERROR_HARD_FAULT, // 硬件错误
    KERNEL_ERROR_TASK_RETURN, // 任务返回错误
} KernelErrorType_t;

// -----------------------------
// 核心对象句柄
// -----------------------------
typedef struct Task_t *TaskHandle_t; // 任务句柄
typedef struct Mutex_t *MutexHandle_t; // 互斥锁句柄
typedef void *QueueHandle_t; // 队列句柄
typedef struct Semaphore_t *SemaphoreHandle_t; // 信号量句柄

// -----------------------------
// 全局内核变量
// -----------------------------
extern TaskHandle_t currentTask; // 当前运行的任务
extern TaskHandle_t idleTask; // 空闲任务
extern volatile uint8_t g_scheduler_started; // 调度器是否已启动

// =============================
// System Core API
// =============================
/**
 * @brief 初始化MyRTOS内核
 * @details 初始化内核相关数据结构和变量
 */
void MyRTOS_Init(void);

/**
 * @brief 执行任务调度
 * @details 根据任务优先级和状态选择下一个要运行的任务
 */
void MyRTOS_Schedule(void);

/**
 * @brief 启动任务调度器
 * @param idle_task_hook 空闲任务钩子函数指针
 */
void Task_StartScheduler(void (*idle_task_hook)(void *));

/**
 * @brief 获取系统当前时钟节拍数
 * @return 当前系统时钟节拍数
 */
uint64_t MyRTOS_GetTick(void);

/**
 * @brief 系统时钟节拍处理函数
 * @details 由系统定时器中断调用，用于更新系统时钟和处理延时任务
 */
int MyRTOS_Tick_Handler(void);

/**
 * @brief 检查调度器是否正在运行
 * @return 1表示调度器正在运行，0表示未运行
 */
uint8_t MyRTOS_Schedule_IsRunning(void);

/**
 * @brief 报告内核严重错误。
 *        此函数由平台层或内部检查调用，用于通知内核发生了致命事件，
 *        然后以内钩子事件的形式广播出去。
 * @param error_type 发生的错误类型。
 * @param p_context  指向上下文特定数据的指针（例如，对于栈溢出，
 *                   指向违规任务的TCB）。
 */

void MyRTOS_ReportError(KernelErrorType_t error_type, void *p_context);

// =============================
// 内核内存管理 API
// =============================
/**
 * @brief 内核内存分配函数
 * @param wantedSize 需要分配的内存大小(字节)
 * @return 成功时返回指向分配内存的指针，失败时返回NULL
 */
void *MyRTOS_Malloc(size_t wantedSize);

/**
 * @brief 内核内存释放函数
 * @param pv 指向需要释放的内存块的指针
 */
void MyRTOS_Free(void *pv);

// =============================
// 任务管理 API
// =============================
/**
 * @brief 创建一个新任务
 * @param func 任务函数指针
 * @param taskName 任务名称
 * @param stack_size 任务堆栈大小(字节)
 * @param param 传递给任务函数的参数
 * @param priority 任务优先级
 * @return 成功时返回任务句柄，失败时返回NULL
 */
TaskHandle_t Task_Create(void (*func)(void *), const char *taskName, uint16_t stack_size, void *param,
                         uint8_t priority);

/**
 * @brief 删除指定任务
 * @param task_h 要删除的任务句柄
 * @return 0表示成功，非0表示失败
 */
int Task_Delete(TaskHandle_t task_h);

/**
 * @brief 延时当前任务指定的时钟节拍数
 * @param tick 需要延时的时钟节拍数
 */
void Task_Delay(uint32_t tick);

/**
 * @brief 通知指定任务解除阻塞
 * @param task_h 被通知的任务句柄
 * @return 0表示成功，非0表示失败
 */
int Task_Notify(TaskHandle_t task_h);

/**
 * @brief 从中断服务例程中通知指定任务解除阻塞
 * @param task_h 被通知的任务句柄
 * @param higherPriorityTaskWoken 用于指示是否有更高优先级任务被唤醒
 * @return 0表示成功，非0表示失败
 */
int Task_NotifyFromISR(TaskHandle_t task_h, int *higherPriorityTaskWoken);

/**
 * @brief 当前任务进入等待状态
 * @details 将当前任务置于阻塞状态，直到被其他任务或中断唤醒
 */
void Task_Wait(void);


/**
 * @brief 向指定任务发送一个或多个信号。
 * @details 这是一个非阻塞操作。它会以原子方式更新目标任务的信号状态，
 *          并唤醒可能正在等待这些信号的目标任务。
 * @param target_task 接收信号的目标任务句柄。
 * @param signals     要发送的信号位掩码。例如: (SIGNAL_A | SIGNAL_B)。
 * @return int 0 表示成功, -1 表示目标任务句柄无效。
 */
int Task_SendSignal(TaskHandle_t target_task, uint32_t signals);

/**
 * @brief 从中断服务例程(ISR)中向指定任务发送一个或多个信号。
 * @param target_task 接收信号的目标任务句柄。
 * @param signals     要发送的信号位掩码。
 * @param higherPriorityTaskWoken 指向一个int的指针，如果此操作导致一个比当前任务
 *                                      更高优先级的任务变为就绪，该值将被设置为1。
 * @return int 0 表示成功, -1 表示参数无效。
 */
int Task_SendSignalFromISR(TaskHandle_t target_task, uint32_t signals, int *higherPriorityTaskWoken);

/**
 * @brief 使当前任务阻塞，直到接收到指定的信号。
 * @param signal_mask 要等待的信号的位掩码。
 * @param block_ticks 阻塞等待的最大时钟节拍数 (0:不阻塞, MYRTOS_MAX_DELAY:永久阻塞)。
 * @param options     等待选项，可以是 SIGNAL_WAIT_ANY, SIGNAL_WAIT_ALL,
 *                         SIGNAL_CLEAR_ON_EXIT 的组合。
 * @return uint32_t   返回任务被唤醒时，所有待处理的信号位掩码 (signals_pending)。
 *                    如果超时，返回 0。
 */
uint32_t Task_WaitSignal(uint32_t signal_mask, uint32_t block_ticks, uint32_t options);

/**
 * @brief 清除一个任务中指定的待处理信号位。
 * @details 当 Task_WaitSignal 不使用 SIGNAL_CLEAR_ON_EXIT 选项时，
 *          任务需要手动调用此函数来清除已处理的信号。
 * @param task_to_clear 目标任务句柄。如果为NULL，则清除当前任务的信号。
 * @param signals_to_clear 要清除的信号位掩码。
 * @return int 0 表示成功, -1 表示任务句柄无效。
 */
int Task_ClearSignal(TaskHandle_t task_to_clear, uint32_t signals_to_clear);



/**
 * @brief 获取指定任务的当前状态
 * @param task_h 任务句柄
 * @return 任务的当前状态
 */
TaskState_t Task_GetState(TaskHandle_t task_h);

/**
 * @brief 获取指定任务的优先级
 * @param task_h 任务句柄
 * @return 任务优先级
 */
uint8_t Task_GetPriority(TaskHandle_t task_h);

/**
 * @brief 获取当前运行任务的句柄
 * @return 当前任务的句柄
 */
TaskHandle_t Task_GetCurrentTaskHandle(void);

/**
 * @brief 获取指定任务的ID
 * @param task_h 任务句柄
 * @return 任务ID
 */
uint32_t Task_GetId(TaskHandle_t task_h);

/**
 * @brief 获取任务的名称。
 * @param task_h 要查询的任务句柄, NULL 表示当前任务
 * @return 返回指向任务名称字符串的指针。如果句柄无效，可能返回NULL或空字符串。
 */
char *Task_GetName(TaskHandle_t task_h);

/**
 * @brief 根据任务名称查找任务句柄。
 * @param taskName 要查找的任务的名称字符串。
 * @return 如果找到，则返回任务的句柄；如果未找到，则返回 NULL。
 */
TaskHandle_t Task_FindByName(const char *taskName);


// =============================
// 队列管理 API
// =============================
/**
 * @brief 创建一个队列
 * @param length 队列长度(可存储的项目数)
 * @param itemSize 队列中每个项目的大小(字节)
 * @return 成功时返回队列句柄，失败时返回NULL
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize);

/**
 * @brief 删除指定队列
 * @param delQueue 要删除的队列句柄
 */
void Queue_Delete(QueueHandle_t delQueue);

/**
 * @brief 向队列发送数据
 * @param queue 队列句柄
 * @param item 指向要发送数据的指针
 * @param block_ticks 在队列满时等待的时钟节拍数(0表示不等待)
 * @return 0表示成功，非0表示失败或超时
 */
int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks);

/**
 * @brief 从队列接收数据
 * @param queue 队列句柄
 * @param buffer 用于存储接收数据的缓冲区指针
 * @param block_ticks 在队列空时等待的时钟节拍数(0表示不等待)
 * @return 0表示成功，非0表示失败或超时
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks);

// =============================
// 互斥锁管理 API
// =============================
/**
 * @brief 创建一个互斥锁
 * @return 成功时返回互斥锁句柄，失败时返回NULL
 */
MutexHandle_t Mutex_Create(void);

/**
 * @brief 删除指定互斥锁
 * @param mutex 要删除的互斥锁句柄
 */
void Mutex_Delete(MutexHandle_t mutex);

/**
 * @brief 获取互斥锁(阻塞)
 * @param mutex 互斥锁句柄
 */
void Mutex_Lock(MutexHandle_t mutex);

/**
 * @brief 在指定时间内尝试获取互斥锁
 * @param mutex 互斥锁句柄
 * @param block_ticks 等待的最大时钟节拍数(0表示不等待)
 * @return 0表示成功获取锁，非0表示失败或超时
 */
int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks);

/**
 * @brief 释放互斥锁
 * @param mutex 互斥锁句柄
 */
void Mutex_Unlock(MutexHandle_t mutex);

/**
 * @brief 获取递归互斥锁
 * @param mutex 互斥锁句柄
 * @note 同一个任务可以多次获取递归互斥锁
 */
void Mutex_Lock_Recursive(MutexHandle_t mutex);

/**
 * @brief 释放递归互斥锁
 * @param mutex 互斥锁句柄
 * @note 必须与Mutex_Lock_Recursive配对使用
 */
void Mutex_Unlock_Recursive(MutexHandle_t mutex);

// =============================
// 信号量管理 API
// =============================
/**
 * @brief 创建一个信号量
 * @param maxCount 信号量最大计数值
 * @param initialCount 信号量初始计数值
 * @return 成功时返回信号量句柄，失败时返回NULL
 */
SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount);

/**
 * @brief 删除指定信号量
 * @param semaphore 要删除的信号量句柄
 */
void Semaphore_Delete(SemaphoreHandle_t semaphore);

/**
 * @brief 获取信号量
 * @param semaphore 信号量句柄
 * @param block_ticks 等待的最大时钟节拍数(0表示不等待)
 * @return 0表示成功获取信号量，非0表示失败或超时
 */
int Semaphore_Take(SemaphoreHandle_t semaphore, uint32_t block_ticks);

/**
 * @brief 释放信号量
 * @param semaphore 信号量句柄
 * @return 0表示成功，非0表示失败
 */
int Semaphore_Give(SemaphoreHandle_t semaphore);

/**
 * @brief 从中断服务例程中释放信号量
 * @param semaphore 信号量句柄
 * @param higherPriorityTaskWoken 用于指示是否有更高优先级任务被唤醒
 * @return 0表示成功，非0表示失败
 */
int Semaphore_GiveFromISR(SemaphoreHandle_t semaphore, int *higherPriorityTaskWoken);


#endif // MYRTOS_H
