/**
 * @file myrtos_init.c
 * @brief MyRTOS 初始化模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 外部函数声明
 *===========================================================================*/
extern void scheduler_init(void);

/*===========================================================================*
 * 全局变量定义
 *===========================================================================*/

// 调度器是否已启动的标志
volatile uint8_t g_scheduler_started = 0;
// 临界区嵌套计数
volatile uint32_t criticalNestingCount = 0;
// 所有已创建任务的链表头
TaskHandle_t allTaskListHead = NULL;
// 当前正在运行的任务的句柄
TaskHandle_t currentTask = NULL;
// 空闲任务的句柄
TaskHandle_t idleTask = NULL;

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

/**
 * @brief 初始化RTOS内核
 * @note  此函数必须在创建任何任务或启动调度器之前调用。
 *        它会初始化所有的内核数据结构。
 */
void MyRTOS_Init(void) {
    allTaskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    scheduler_init();
}

/**
 * @brief 启动RTOS调度器
 * @note  此函数不会返回。它会创建空闲任务，然后启动第一个任务的执行。
 *        在此之后，任务调度由内核接管。
 * @param idle_task_func 空闲任务的函数指针。当没有其他任务可运行时，将执行此任务。
 */
void Task_StartScheduler(void (*idle_task_func)(void *)) {
    // 必须提供一个有效的空闲任务函数
    if (idle_task_func == NULL) {
        while (1);
    }
    // 创建空闲任务，其优先级为最低
    idleTask = Task_Create(idle_task_func, "IDLE", 128, NULL, 0);
    if (idleTask == NULL) {
        // 如果空闲任务创建失败，系统无法继续
        while (1);
    }
    // 标记调度器已启动
    g_scheduler_started = 1;
    // 手动调用一次调度以选择第一个要运行的任务
    MyRTOS_Schedule();
    // 调用移植层代码来启动调度器（通常是设置第一个任务的堆栈指针并触发SVC/PendSV）
    if (MyRTOS_Port_StartScheduler() != 0) {
        // 此处不应该被执行到
    }
}

/**
 * @brief 报告一个内核级错误
 * @note  此函数通过内核扩展机制广播错误事件，允许调试工具或用户代码捕获和处理这些错误。
 * @param error_type 错误类型
 * @param p_context 与错误相关的上下文数据指针
 */
void MyRTOS_ReportError(KernelErrorType_t error_type, void *p_context) {
    KernelEventData_t eventData;
    // 最好先清空事件数据结构
    memset(&eventData, 0, sizeof(eventData));

    eventData.p_context_data = p_context;

    switch (error_type) {
        case KERNEL_ERROR_STACK_OVERFLOW:
            eventData.eventType = KERNEL_EVENT_HOOK_STACK_OVERFLOW;
            eventData.task = (TaskHandle_t) p_context;
            break;

        case KERNEL_ERROR_MALLOC_FAILED:
            eventData.eventType = KERNEL_EVENT_HOOK_MALLOC_FAILED;
            eventData.mem.size = (size_t) p_context;
            eventData.mem.ptr = NULL;
            break;

        case KERNEL_ERROR_HARD_FAULT:
            eventData.eventType = KERNEL_EVENT_ERROR_HARD_FAULT;
            break;
        case KERNEL_ERROR_TASK_RETURN:
            eventData.eventType = KERNEL_EVENT_ERROR_HARD_FAULT;
            eventData.task = (TaskHandle_t) p_context;
            break;
        default:
            return; // 未知的错误类型，不处理
    }

    broadcast_event(&eventData);
}
