/**
 * @file  MyRTOS_Monitor.h
 * @brief MyRTOS 监控服务 - 公共接口
 * @details 提供获取任务运行时状态、CPU使用率、栈使用和堆内存统计的功能。
 */
#ifndef MYRTOS_MONITOR_H
#define MYRTOS_MONITOR_H

#include "MyRTOS_Service_Config.h"

#ifndef  MYRTOS_SERVICE_MONITOR_ENABLE
#define MYRTOS_SERVICE_MONITOR_ENABLE 0
#endif

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1

#include "MyRTOS.h"
#include "MyRTOS_Monitor_Standards.h"

/**
 * @brief 任务统计信息结构体 (对外暴露)。
 */
typedef struct {
    TaskHandle_t task_handle;           // 任务句柄
    const char *task_name;              // 任务名称
    TaskState_t state;                  // 当前状态 (Running, Ready, Blocked...)
    uint8_t current_priority;           // 当前优先级 (可能因优先级继承而改变)
    uint8_t base_priority;              // 基础优先级
    uint32_t stack_size_bytes;          // 总栈大小 (字节)
    uint32_t stack_high_water_mark_bytes; // 栈历史最高使用量 (字节)，值越小表示剩余栈空间越多
    uint64_t total_runtime;             // 总运行时间 (单位: 高精度timer ticks)
    uint32_t cpu_usage_permille;        // CPU使用率 (千分比)，需由调用者在两个时间点上计算差值得出
} TaskStats_t;


/**
 * @brief 堆内存统计信息结构体 (对外暴露)。
 */
typedef struct {
    size_t total_heap_size;             // 总堆大小 (字节)
    size_t free_bytes_remaining;        // 当前剩余空闲字节数
    size_t minimum_ever_free_bytes;     // 历史最小剩余字节数 (堆的水线)
} HeapStats_t;


/**
 * @brief Monitor 初始化配置结构体 (用于依赖注入)。
 */
typedef struct {
    /** 必须提供一个获取高精度时钟值的函数指针，用于统计任务运行时间 */
    MonitorGetHiresTimerValueFn get_hires_timer_value;
} MonitorConfig_t;

/**
 * @brief 初始化Monitor服务。
 * @details 此函数会向内核注册事件监听器，以被动方式收集统计数据，开销极小。
 * @param config [in] 指向Monitor配置的指针。
 * @return int 0 成功, -1 失败 (如配置为空)。
 */
int Monitor_Init(const MonitorConfig_t *config);

/**
 * @brief (内核API扩展) 获取任务链表中的下一个任务句柄。
 * @details 用于遍历系统中所有任务。
 * @param previous_handle 上一个任务的
* @return TaskHandle_t 下一个任务的句柄, 或 NULL 如果已遍历完。
 */
TaskHandle_t Monitor_GetNextTask(TaskHandle_t previous_handle);

/**
 * @brief 获取指定任务的详细统计信息。
 * @param task_h      [in]  要查询的任务句柄。
 * @param p_stats_out [out] 用于填充统计信息的结构体指针。
 * @return int 0 成功, -1 失败 (如句柄无效)。
 */
int Monitor_GetTaskInfo(TaskHandle_t task_h, TaskStats_t *p_stats_out);

/**
 * @brief 获取堆内存的统计信息。
 * @param p_stats_out [out] 用于填充堆统计信息的结构体指针。
 */
void Monitor_GetHeapStats(HeapStats_t *p_stats_out);

#endif // MYRTOS_MONITOR_ENABLE

#endif // MYRTOS_MONITOR_H
