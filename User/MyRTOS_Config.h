//
// Created by XiaoXiu on 8/28/2025.
//

#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

//====================== 内核核心配置 ======================
#define MY_RTOS_MAX_PRIORITIES              (16)      // 最大支持的优先级数量
#define MY_RTOS_TICK_RATE_HZ                (1000)    // 系统Tick频率 (Hz)
#define MY_RTOS_MAX_DELAY                   (0xFFFFFFFFU) // 最大延时ticks
#define MY_RTOS_TASK_NAME_MAX_LEN           (16)       // 任务名称最大长度

//====================== 内存管理配置 ======================
#define RTOS_MEMORY_POOL_SIZE               (32 * 1024) // 内核使用的静态内存池大小 (bytes)
#define HEAP_BYTE_ALIGNMENT                 (8)         // 内存对齐字节数

//====================== 系统服务配置 ======================

// --- 标准I/O (printf) 服务 ---
#define MY_RTOS_USE_STDIO                   1 // 1: 启用printf重定向服务; 0: 禁用
#if (MY_RTOS_USE_STDIO == 1)
// 配置用于保护I/O输出的互斥锁等待时间
#define MY_RTOS_IO_MUTEX_TIMEOUT_MS     100
#endif

//====================== 日志与I/O配置 ======================
#define MY_RTOS_USE_LOG                     1 // 1: 启用日志和printf服务; 0: 禁用
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


//====================== 运行时统计与监视器配置 ======================

// --- 运行时统计功能 ---
// 1: 开启运行时统计功能; 0: 关闭。这是监视器的基础，必须开启。
#define MY_RTOS_GENERATE_RUN_TIME_STATS     1

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
// --- 系统监视器 (Monitor) ---
// 1: 启用系统监视器模块; 0: 禁用
#define MY_RTOS_USE_MONITOR             1

#if (MY_RTOS_USE_MONITOR == 1)
// 监视器任务的相关配置
#define MY_RTOS_MONITOR_TASK_PRIORITY      (1)
#define MY_RTOS_MONITOR_TASK_STACK_SIZE    (1024) // 需要较大栈来容纳缓冲区
#define MY_RTOS_MONITOR_TASK_PERIOD_MS     (1000) // 监视器刷新周期

// 监视器用于格式化输出的缓冲区大小
#define MY_RTOS_MONITOR_BUFFER_SIZE        (2048)
#endif
#endif


#endif // MYRTOS_CONFIG_H
