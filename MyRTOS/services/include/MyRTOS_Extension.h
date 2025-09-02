//
// Created by XiaoXiu on 8/30/2025.
//
#ifndef MYRTOS_EXTENSION_H
#define MYRTOS_EXTENSION_H
#include "MyRTOS.h"

#define MAX_KERNEL_EXTENSIONS 8

/**
 * @brief 内核事件类型枚举
 *
 * 内核将在关键操作点广播这些事件。
 * 扩展模块可以监听这些事件来执行相应的功能。
 */
typedef enum {
    // 系统事件
    KERNEL_EVENT_TICK, // 系统Tick中断发生后
    // 任务生命周期事件
    KERNEL_EVENT_TASK_CREATE, // 任务创建成功后
    KERNEL_EVENT_TASK_DELETE, // 任务被删除前
    KERNEL_EVENT_TASK_SWITCH_OUT, // 任务即将被换出
    KERNEL_EVENT_TASK_SWITCH_IN, // 任务即将被换入
    // 内存管理事件
    KERNEL_EVENT_MALLOC, // 内存分配后
    KERNEL_EVENT_FREE, // 内存释放后
    // 这里是一些钩子事件
    KERNEL_EVENT_HOOK_MALLOC_FAILED, // 内存分配失败
    KERNEL_EVENT_HOOK_STACK_OVERFLOW,
    KERNEL_EVENT_ERROR_HARD_FAULT
    // 可以要添加更多事件 不过我还没注入进去喵喵喵
    // KERNEL_EVENT_QUEUE_SEND,
    // KERNEL_EVENT_QUEUE_RECEIVE,
} KernelEventType_t;


/**
 * @brief 内核事件上下文数据结构
 *
 * 当内核广播事件时，会传递一个指向此结构的指针，
 * 包含了与该事件相关的上下文信息。
 */
typedef struct {
    KernelEventType_t eventType; // 事件类型
    TaskHandle_t task; // 关联的任务句柄 (适用于任务相关事件)
    // 用于内存事件
    struct {
        void *ptr; // 分配/释放的内存指针
        size_t size; // 请求/块的大小
    } mem;

    void *p_context_data;
    // 可以扩展更多上下文数据
} KernelEventData_t;


/**
 * @brief 内核扩展回调函数类型
 *
 * 外部扩展模块需要实现此类型的函数来处理内核事件。
 * @param pEventData 指向包含事件信息的结构体。
 */
typedef void (*KernelExtensionCallback_t)(const KernelEventData_t *pEventData);


/**
 * @brief 注册一个内核扩展
 *
 * 外部模块（如运行时统计、调试跟踪器）通过此函数
 * 将自己的回调函数注册到内核。
 *
 * @param callback 要注册的回调函数指针。
 * @return 0 成功, -1 失败 (例如，注册表已满)。
 */
int MyRTOS_RegisterExtension(KernelExtensionCallback_t callback);

/**
 * @brief 注销一个内核扩展
 *
 * @param callback 要注销的回调函数指针。
 * @return 0 成功, -1 失败 (例如，未找到该回调)。
 */
int MyRTOS_UnregisterExtension(KernelExtensionCallback_t callback);


#endif // MYRTOS_EXTENSION_H
