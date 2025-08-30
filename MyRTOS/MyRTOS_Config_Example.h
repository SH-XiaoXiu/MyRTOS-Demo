#if 0
#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

//======================================================================
//                    PLATFORM & ARCHITECTURE
//======================================================================
// 目标硬件平台的底层头文件
#include "gd32f4xx.h"

// 平台相关的宏，用于临界区保护和任务切换的强制调度
// criticalNestingCount 用于支持嵌套的临界区
extern volatile uint32_t criticalNestingCount;

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


//======================================================================
//                       CORE KERNEL
//======================================================================
// RTOS内核的基础配置，系统运行的必需项
#define MY_RTOS_MAX_PRIORITIES              (16)      // 最大支持的优先级数量
#define MY_RTOS_MAX_CONCURRENT_TASKS        (32)      // 最大并发任务数，当前实现下不能超过64
#define MY_RTOS_TICK_RATE_HZ                (1000)    // 系统Tick频率 (Hz)
#define MY_RTOS_MAX_DELAY                   (0xFFFFFFFFU) // 用于API的最大延时ticks
#define MY_RTOS_TASK_NAME_MAX_LEN           (16)      // 任务名称字符串的最大长度

//======================================================================
//                    MEMORY MANAGEMENT
//======================================================================
#define RTOS_MEMORY_POOL_SIZE               (64 * 1024) // 内核管理的静态内存池总大小 (bytes)
#define HEAP_BYTE_ALIGNMENT                 (8)         // 堆内存分配的对齐字节数

//======================================================================
//                  OPTIONAL KERNEL MODULES
//======================================================================
// 内核中的可选功能模块，可配置以裁剪系统大小
#define MY_RTOS_USE_SOFTWARE_TIMERS         1 // 1: 启用软件定时器服务; 0: 禁用
#define MY_RTOS_GENERATE_RUN_TIME_STATS     1 // 1: 启用运行时统计功能; 0: 禁用

//======================================================================
//                      SYSTEM SERVICES & COMPONENTS
//======================================================================
// 构建在内核之上的高级服务与标准组件，可按需启用
#define MY_RTOS_USE_STDIO                   1 // 1: 启用标准I/O服务，是交互式服务的基础
#define MY_RTOS_USE_LOG                     1 // 1: 启用日志服务
#define MY_RTOS_USE_SHELL                   1 // 1: 启用Shell(终端)服务
#define MY_RTOS_USE_MONITOR                 1 // 1: 启用标准的系统监视器组件

//======================================================================
//                 SERVICE-SPECIFIC CONFIGURATIONS
//======================================================================

#if (MY_RTOS_USE_LOG == 1)
// --- 日志级别定义 ---
#define SYS_LOG_LEVEL_NONE              0
#define SYS_LOG_LEVEL_ERROR             1
#define SYS_LOG_LEVEL_WARN              2
#define SYS_LOG_LEVEL_INFO              3
#define SYS_LOG_LEVEL_DEBUG             4
// 系统日志和用户日志的默认过滤级别
#define SYS_LOG_LEVEL                   SYS_LOG_LEVEL_ERROR
#define USER_LOG_LEVEL                  SYS_LOG_LEVEL_INFO
// --- 异步日志系统配置 ---
#define SYS_LOG_QUEUE_LENGTH            30   // 日志消息队列的长度
#define SYS_LOG_MAX_MSG_LENGTH          128  // 单条日志消息的最大长度
#define SYS_LOG_TASK_PRIORITY           1    // 日志任务的优先级
#define SYS_LOG_TASK_STACK_SIZE         512  // 日志任务的栈大小 (words)
#else
#define SYS_LOG_LEVEL_NONE              0
#define SYS_LOG_LEVEL_ERROR             1
#define SYS_LOG_LEVEL_WARN              2
#define SYS_LOG_LEVEL_INFO              3
#define SYS_LOG_LEVEL_DEBUG             4
#define SYS_LOG_LEVEL                   SYS_LOG_LEVEL_DEBUG
#define USER_LOG_LEVEL                  SYS_LOG_LEVEL_DEBUG
#endif

#if (MY_RTOS_USE_MONITOR == 1)
#define SYS_MONITOR_TASK_PRIORITY       (1)    // 监视器任务通常优先级较低
#define SYS_MONITOR_TASK_STACK_SIZE     (1024) // 需要较大栈来处理字符串格式化
#define SYS_MONITOR_REFRESH_PERIOD_MS   (500) // 刷新周期 (ms)
#define MAX_TASKS_FOR_STATS             (MY_RTOS_TASK_NAME_MAX_LEN)
#endif

#if (MY_RTOS_USE_SHELL == 1)
#define SYS_SHELL_TASK_PRIORITY         (2)    // Shell任务的优先级
#define SYS_SHELL_TASK_STACK_SIZE       (1024) // Shell任务的栈大小 (words)
#define SYS_SHELL_MAX_CMD_LENGTH        (64)   // Shell接收命令的最大长度
#define SYS_SHELL_MAX_ARGS              (8)    // Shell命令解析后的最大参数个数
#define SYS_SHELL_PROMPT                "MyRTOS> " // Shell的命令行提示符
#endif

//======================================================================
//                      HARDWARE DRIVER MAPPING
//======================================================================
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
// 定义项目中使用的硬件定时器列表
// 格式: USE_TIMER(逻辑ID, 物理定时器, 预留参数)
#define MY_RTOS_TIMER_DEVICE_LIST \
        USE_TIMER(PERF_COUNTER_TIMER, TIMER1, 0) \
        USE_TIMER(USER_APP_TIMER,     TIMER2, 0)

// 将运行时统计功能绑定到一个具体的硬件定时器逻辑ID
#define MY_RTOS_STATS_TIMER_ID          TIMER_ID_PERF_COUNTER_TIMER
// 期望该定时器的计数频率 (Hz)，驱动层会据此计算预分频
#define MY_RTOS_STATS_TIMER_FREQ_HZ     1000000
#endif

//======================================================================
//                     CONFIGURATION DEPENDENCY CHECKS
//======================================================================
// 编译时检查，防止不合法的配置组合
#if (MY_RTOS_USE_LOG == 1 && MY_RTOS_USE_STDIO == 0)
#error "Log Service requires StdIO Service. Please enable MY_RTOS_USE_STDIO."
#endif

#if (MY_RTOS_USE_SHELL == 1 && MY_RTOS_USE_STDIO == 0)
#error "Shell Service requires StdIO Service. Please enable MY_RTOS_USE_STDIO."
#endif

#if (MY_RTOS_USE_MONITOR == 1 && MY_RTOS_USE_STDIO == 0)
#error "Default Monitor Component implementation requires StdIO Service. Please enable MY_RTOS_USE_STDIO."
#endif

#if (MY_RTOS_USE_MONITOR == 1 && MY_RTOS_GENERATE_RUN_TIME_STATS == 0)
#error "Monitor Component requires Run-Time Stats. Please enable MY_RTOS_GENERATE_RUN_TIME_STATS."
#endif


#endif // MYRTOS_CONFIG_H

#endif
