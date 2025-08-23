#ifndef MYRTOS_H
#define MYRTOS_H

#include <stdint.h>

#define MAX_TASKS   8
#define STACK_SIZE  256
#define IDLE_TASK_ID (MAX_TASKS-1)


// 调试输出开关
#define DEBUG_PRINT 0  // 设置为1开启调试输出,0关闭
//调试输出函数
#if DEBUG_PRINT
#include <stdio.h>
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

// 定义任务状态
typedef enum {
    TASK_STATE_UNUSED, // 未使用
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
    struct Task_t* owner_tcb;
    struct Mutex_t* next_held_mutex;
} Mutex_t;

// 任务控制块 (TCB)
//为了保持汇编代码的兼容性 sp 必须是结构体的第一个成员
typedef struct Task_t {
    void *sp; // 栈指针 (Stack Pointer)
    void (*func)(void *); // 任务函数指针
    void *param; // 任务参数
    volatile uint32_t delay; // 任务延时节拍数
    volatile TaskState_t state; // 任务状态
    uint32_t notification; // 任务通知值
    uint8_t is_waiting_notification; // 是否在等待通知的标志
    uint32_t taskId; // 任务ID
    uint32_t *stack_base; // 栈的基地址，用于释放内存
    struct Task_t *next; // 指向下一个任务的指针
    Mutex_t* held_mutexes_head;
} Task_t;





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

Task_t *Task_Create(void (*func)(void *), void *param);

int Task_Delete(const Task_t *task_h);

void Task_StartScheduler(void);

void Task_Delay(uint32_t tick);

int Task_Notify(uint32_t task_id);

void Task_Wait(void);

void Mutex_Init(Mutex_t *mutex);

void Mutex_Lock(Mutex_t *mutex);

void Mutex_Unlock(Mutex_t *mutex);
#endif //MYRTOS_H
