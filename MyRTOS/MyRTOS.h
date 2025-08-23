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

// 任务控制块 (TCB)
//为了保持汇编代码的兼容性 sp 必须是结构体的第一个成员
typedef struct Task_t {
    void *sp;                        // 栈指针 (Stack Pointer)
    void (*func)(void *);            // 任务函数指针
    void *param;                     // 任务参数
    volatile uint32_t delay;         // 任务延时节拍数
    volatile TaskState_t state;      // 任务状态
    uint32_t notification;           // 任务通知值
    uint8_t is_waiting_notification; // 是否在等待通知的标志
    uint32_t taskId;                 // 任务ID
    uint32_t *stack_base;            // 栈的基地址，用于释放内存
    struct Task_t *next;             // 指向下一个任务的指针
} Task_t;

// 互斥锁结构体
typedef struct {
    volatile int locked; // 锁状态
    volatile uint32_t owner; // 当前持有锁的任务ID
    volatile uint32_t waiting_mask; // 等待该锁的任务掩码 (假设 MAX_TASKS <= 32)
} Mutex_t;

Task_t* Task_Create(void (*func)(void *), void *param);

int Task_Delete(const Task_t *task_h);

void Task_StartScheduler(void);

void Task_Delay(uint32_t tick);

int Task_Notify(uint32_t task_id);

void Task_Wait(void);

void Mutex_Init(Mutex_t *mutex);

void Mutex_Lock(Mutex_t *mutex);

void Mutex_Unlock(Mutex_t *mutex);
#endif //MYRTOS_H
