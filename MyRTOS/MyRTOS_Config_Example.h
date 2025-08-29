#if 0
#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

//====================== 平台与架构定义 ======================
//包含目标硬件平台的底层头文件
#include "gd32f4xx.h"

extern volatile uint32_t criticalNestingCount;
//定义平台相关的宏 (临界区, Yield等)
#define MyRTOS_Port_ENTER_CRITICAL() \
do { \
    __disable_irq(); \
    criticalNestingCount++; \
} while(0)

#define MyRTOS_Port_EXIT_CRITICAL() \
do { \
    if (criticalNestingCount > 0) { \
        criticalNestingCount--; \
        if (criticalNestingCount == 0) { \
            __enable_irq(); \
        } \
    } \
} while(0)

#define MyRTOS_Port_YIELD()                      do { SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; __ISB(); } while(0)


//====================== 内核核心配置 ======================
#define MY_RTOS_MAX_PRIORITIES              (16)      // 最大支持的优先级数量
#define MY_RTOS_TICK_RATE_HZ                (1000)    // 系统Tick频率 (Hz)
#define MY_RTOS_MAX_DELAY                   (0xFFFFFFFFU) // 最大延时ticks
#define MY_RTOS_TASK_NAME_MAX_LEN           (16)       // 任务名称最大长度

//====================== 内存管理配置 ======================
#define RTOS_MEMORY_POOL_SIZE               (32 * 1024) // 内核使用的静态内存池大小 (bytes)
#define HEAP_BYTE_ALIGNMENT                 (8)         // 内存对齐字节数

//====================== 日志与监视器配置 ======================
#define MY_RTOS_USE_LOG                     1 // 1: 启用日志服务; 0: 禁用
#define MY_RTOS_USE_MONITOR                 1 // 1: 启用系统监视器模块; 0: 禁用
#if MY_RTOS_USE_MONITOR
#define MY_RTOS_MONITOR_KERNEL_LOG          1 //1: 启用内核日志; 0: 禁用内核日志
#endif


#if (MY_RTOS_USE_LOG == 1)
// --- 日志级别定义 ---
#define SYS_LOG_LEVEL_NONE              0 // 不打印任何日志
#define SYS_LOG_LEVEL_ERROR             1 // 只打印错误
#define SYS_LOG_LEVEL_WARN              2 // 打印错误和警告
#define SYS_LOG_LEVEL_INFO              3 // 打印错误、警告和信息
#define SYS_LOG_LEVEL_DEBUG             4 // 打印所有级别

// 设置当前系统的日志级别
#define SYS_LOG_LEVEL                   SYS_LOG_LEVEL_INFO

// --- 异步日志系统配置 ---
#define SYS_LOG_QUEUE_LENGTH            30
#define SYS_LOG_MAX_MSG_LENGTH          128
#define SYS_LOG_TASK_PRIORITY           1
#define SYS_LOG_TASK_STACK_SIZE         512
#endif

#if (MY_RTOS_USE_MONITOR == 1)
// 监视器任务的相关配置
#define MY_RTOS_MONITOR_TASK_PRIORITY      (1)
#define MY_RTOS_MONITOR_TASK_STACK_SIZE    (1024) // 需要较大栈来容纳缓冲区
#define MY_RTOS_MONITOR_TASK_PERIOD_MS     (500) // 监视器刷新周期
#define MY_RTOS_MONITOR_BUFFER_SIZE        (2048)
#define MAX_TASKS_FOR_STATS                (32) //最大监视任务数
#endif

//====================== 运行时统计配置 ======================
// 1: 开启运行时统计功能; 0: 关闭。这是监视器的基础。
#define MY_RTOS_GENERATE_RUN_TIME_STATS     1

//====================== 硬件驱动配置 ======================

// --- 定时器驱动配置 ---
// 定义想要使用的定时器“逻辑实例”
// 格式：USE_TIMER(逻辑ID, 物理定时器, 预留参数)
#define MY_RTOS_TIMER_DEVICE_LIST \
    USE_TIMER(PERF_COUNTER_TIMER, TIMER1, 0) \
    USE_TIMER(USER_APP_TIMER,     TIMER2, 0)

// --- 模块与驱动映射 ---
// 为运行时统计功能分配一个高精度时钟源
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
#define MY_RTOS_STATS_TIMER_ID          TIMER_ID_PERF_COUNTER_TIMER
#define MY_RTOS_STATS_TIMER_FREQ_HZ     1000000 // 期望的频率 (1MHz)
#endif

#endif // MYRTOS_CONFIG_H

#endif
