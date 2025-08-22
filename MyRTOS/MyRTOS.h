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
typedef struct {
    void *sp; // 任务栈顶指针 (必须是第一个成员，方便汇编)
    TaskState_t state; // 任务当前状态
    void (*func)(void *); // 任务函数指针
    void *param; // 任务函数参数
    uint32_t delay; // 延时节拍数
    uint32_t notification; //通知值 -保留字段
    int is_waiting_notification; // 标记是否在等待通知
} Task_t;

// 互斥锁结构体
typedef struct {
    volatile int locked; // 锁状态
    volatile uint32_t owner; // 当前持有锁的任务ID
    volatile uint32_t waiting_mask; // 等待该锁的任务掩码 (假设 MAX_TASKS <= 32)
} Mutex_t;

int Task_Create(uint32_t taskId, void (*func)(void *), void *param);

void Task_StartScheduler(void);

void Task_Delay(uint32_t tick);

int Task_Notify(uint32_t task_id);

void Task_Wait(void);

void Mutex_Init(Mutex_t *mutex);

void Mutex_Lock(Mutex_t *mutex);

void Mutex_Unlock(Mutex_t *mutex);
#endif //MYRTOS_H
